// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0)
		consputc(buf[i]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint *argp;
	char *s;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	argp = (uint*)(void*)(&fmt + 1);
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(*argp++, 10, 1);
			break;
		case 'x':
		case 'p':
			printint(*argp++, 16, 0);
			break;
		case 's':
			if((s = (char*)*argp++) == 0)
				s = "(null)";
			for(; *s; s++)
				consputc(*s);
			break;
		case '%':
			consputc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			consputc(c);
			break;
		}
	}

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static int currentPosition;
static int lastSavedPosition;
static int isTablePrinted = 0;
static int isContentSaved = 0;
static ushort removedContent[81];
static char colorMode[7];

static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	}
	else if(colorMode[0] != 0) {
		crt[pos++] = (c&0xff) | getColor(colorMode);
	}
	else
		crt[pos++] = (c&0xff) | 0x0700;  // black on white

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);
	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x
#define A(x) ((x) - '@')

void
consoleintr(int (*getc)(void))
{
	int c, doprocdump = 0;
	static int tableFlag = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w && !tableFlag){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case A('c'):
			if(!tableFlag) {
				printTable();
				tableFlag = 1;
			}
			else {
				removeTable();
				tableFlag = 0;
			}
			break;
		default:
			if(tableFlag) {
				moveCursor(c);
				break;
			}

			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;
				consputc(c);
				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}

int
getCursorPosition(){
	int pos, pos_high, pos_low;

	outb(0x3D4, 0x0E);
	pos_high = inb(0x3D5);
	outb(0x3D4, 0x0F);
	pos_low = inb(0x3D5);
	pos = (pos_high << 8) | pos_low;

	return pos;

}

int getTableStartingPosition(int cursorPosition) {
	for(int limit = 880; limit < 2000; limit += 80){
		if(limit - cursorPosition < 9 && limit - cursorPosition > 0)
			return cursorPosition - 729;
	}

	return cursorPosition - 719;
}

void printTable() {
	int cursorPosition = getCursorPosition();
	int startPosition = getTableStartingPosition(cursorPosition);
	int index = 0;
	char table[] = "_________:WHT BLK:_________:PUR WHT:_________:RED AQU:_________:WHT YEL:_________:";
	int tableSize = 81;

	if(!isTablePrinted) {
		currentPosition = startPosition + 89;
		isTablePrinted = 1;	
	}
	
	for(int i = 0; i < tableSize; i++) {
		if(!isContentSaved) {
			removedContent[i] = crt[startPosition];
		}
		if(i != 0 && i % 9 == 0) startPosition += 71;
		crt[startPosition++] = (table[i] & 0xff) | 0x7000;
	}
	isContentSaved = 1;
}

void removeTable() {
	int cursorPosition = getCursorPosition();
	int startPosition = getTableStartingPosition(cursorPosition);
	int tableSize = 81;	
		
	for(int i = 0; i < tableSize; i++) {
		if(i != 0 && i % 9 == 0) startPosition += 71;
		crt[startPosition++] = removedContent[i];
	}
	isTablePrinted = 0;
	isContentSaved = 0;
}


void moveCursor(char direction) {
	int cursorPosition = getCursorPosition();
	int tableStart = getTableStartingPosition(cursorPosition);
	int tableEnd = tableStart + 560;
	int index = 0;

	if(direction == 'w') {
		if(currentPosition - 169 < tableStart) currentPosition = tableEnd;
		else currentPosition -= 169;
		
		printTable();
		for(int i = 0; i < 9; i++) {
			if(i > 0 && i < 8) colorMode[index++] = (char)crt[currentPosition];
			crt[currentPosition] = (crt[currentPosition] & 0xff) | 0x2700;
			currentPosition++;
		}
	}
	else if(direction == 's') {
		if(currentPosition + 151 > tableEnd) currentPosition = tableStart + 80;
		else currentPosition += 151;

		printTable();
		for(int i = 0; i < 9; i++) {
			if(i > 0 && i < 8) colorMode[index++] = (char)crt[currentPosition];
			crt[currentPosition] = (crt[currentPosition] & 0xff) | 0x2700;
			currentPosition++;
		}	
	}
}

int compareTwoString(char *a, char *b)
{
    int flag = 0;
    while (*a != '\0' && *b != '\0') // while loop
    {
        if (*a != *b)
        {
            flag = 1;
        }
        a++;
        b++;
    }
 if(*a!='\0'||*b!='\0')
       return 1;
    if (flag == 0)
        return 0;
    else
        return 1;
}

#define WHT_BLK 0x7000
#define PUR_WHT 0x5700
#define RED_AQU 0x4300
#define WHT_YEL 0x7600

int getColor(char colorMode[]) {
	if(!compareTwoString(colorMode, "WHT BLK")) return WHT_BLK;
	else if(!compareTwoString(colorMode, "PUR WHT")) return PUR_WHT;
	else if(!compareTwoString(colorMode, "RED AQU")) return RED_AQU;
	else return WHT_YEL;
}
