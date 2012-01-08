
/* 

 An EOR polygon filler for MSX by Antti Silvast, 2011, 2012.

 Compiling:
 - Type make at the command line. 
 
*/


#include <stdio.h>
#include <msxlib.h>
#include <interrupt.h>
#include <math.h>

// the preset sin table
#include "sin_table.inc"

// this array stores the values sin/5 (divisions do not exist on the Z80 processor and are thus slow). 
signed char sini5[256];

// constants for the keyboard reader
sfr at 0xAA keyboard_line;
sfr at 0xA9 keyboard_column_r;

// the time elapsed measured in 1/50 s
int master_frame=0;

// offscreen buffer for drawing the lines and polygons
static unsigned char line_offscreen_buffer[32*64];
 
void Dummy_function()
// This Dummy function is here towards mitigating what seems like a compiler or a library bug.
// The first routine of the function gets overwritten by someone, so we define this to 
// prevent the crash. 
{
	char tmp[256];
	tmp[0]=0;	

}

void vdp_bigcopy(unsigned char *src,unsigned block32)
// Copies a big chunk from the main memory to the video memory
// Routine by Markku Reunanen, 2011
{
    src;block32;
    _asm
    push    de
    push    hl
    push    bc

    ld      l,4(ix) ; Parametrit
    ld      h,5(ix)
    ld      e,6(ix)
    ld      d,7(ix)
    ld      c,#0x98

0$:
    outi
    outi
    outi
    outi
    outi
    outi
    outi
    outi

    outi
    outi
    outi
    outi
    outi
    outi
    outi
    outi

    outi
    outi
    outi
    outi
    outi
    outi
    outi
    outi

    outi
    outi
    outi
    outi
    outi
    outi
    outi
    outi
    dec     de
    ld      a,d
    or      e
    jp      nz,0$

    pop     bc
    pop     hl
    pop     de
    _endasm;
}

void mem_set(void *dest,unsigned char c,unsigned len)
// Formats a main memory with specific value for defined length. 
// Routine by Markku Reunanen, 2011
{
   	dest;c;len;
    	_asm
    	push    de
    	push    hl
    	push    bc

    	ld      l,4(ix) ; Parametrit
    	ld      h,5(ix)
    	ld	c,6(ix)
    	ld      e,7(ix)
    	ld      d,8(ix)
    	push    de

    	srl     d   ; DE/8
    	rr      e
    	srl     d
   	rr      e
    	srl     d
    	rr      e
    	ld      a,d
    	or      e
    	jp      z,2$

1$:
   	ld	(hl),c
	inc	hl
   	ld	(hl),c
	inc	hl
   	ld	(hl),c
	inc	hl
   	ld	(hl),c
	inc	hl
   	ld	(hl),c
	inc	hl
   	ld	(hl),c
	inc	hl
   	ld	(hl),c
	inc	hl
   	ld	(hl),c
	inc	hl

	dec     de
    	ld      a,d
    	or      e
    	jp      nz,1$

2$:
    	pop     de      ; The modulo part
    	ld      a,e
    	and     #7
    	jp      z,4$

	ld	b,a
3$:
    	ld      (hl),c
	inc	hl
    	djnz	3$

4$:
    	pop     bc
    	pop     hl
    	pop     de
    	_endasm;
}

static int name_table_type;
void set_name_table(int type) {
/* Set the name table
   INPUT: 	type	type of the name table (1=left to right, 2=top to bottom)

   Name tables are the "letters" of the MSX video memory, 
   which tell where each graphics character (0..256) lies on the screen. 
*/

	// if name table already the same as requested, do nothing
	if (type==name_table_type) return;
	name_table_type=type;
	
	vdp_address(0x1800); // MSX nametable is at the address 0x1800

	if (type==1) {
		// Set the normal MSX name table left to right
		_asm
		
		ld c,#3
		31$:
		ld b,#0
		ld d,#8
		1$:
		ld e,#32
		2$:
		ld a,b
		out (0x98),a
		inc b
		dec e
		jp nz,2$
		dec d
		jp nz,1$
		dec c
		jp nz,31$
		_endasm;
		
	} else {
		
		_asm
		// Set the pseudolinear top-to bottom state
		ld d,#3
		311$:

		ld b,#0 // B: character number
		ld c,#8
		2031$:
		push bc
		ld e,#32
		202$:
		ld a,b
		out (0x98),a
		add a,#8
		ld b,a
		dec e
		jp nz,202$

		pop bc
		inc b
		dec c
		jp nz,2031$
		dec d
		jp nz,311$
		_endasm;
		
	}
}

int abs(int x) {
// The absolute function
	if (x<0) return (-x); else return x;
}


// The powers of 2 from 2^7 to 2^0. 
unsigned char pow2[8]={128,64,32,16,8,4,2,1};

// variables for the line routine
static unsigned char * buffer;
static unsigned char bit;
static unsigned char len;
static signed char err,e2;
static signed char sy;
static signed char dx,dy;
static unsigned char color;
void line(unsigned char * dest, int x1, int y1, int x2, int y2, unsigned char col) {
/* A line routine
   INPUT:	dest		the buffer to draw the line
		x1,y1		coordinates of the other end
		x2,y2		coordinates of the other end
		col		a color mask (e.g. 0xFF - mask nothing, 85 - mask every 2nd pixel)

   This is a modified Bresenham's line algorithm that only draws a pixel when the X coordinate has changed.
   As will be seen soon, this arrangement is for the EOR filler.
*/

	int t;
	
	// arrange so that x1 is always smaller than x2
	if (x2<x1) {
		t=x1;
		x1=x2;
		x2=t;
		
		t=y1;
		y1=y2;
		y2=t;
	}

	// store parameters
	color=col;
	dx=x2-x1;
	dy=abs(y2-y1);

	// the sign of the Y increment
	if (y1<y2) sy=1; else sy=-1;

	// Bresenham error term
	err=dx-dy;
		
	// Destination
	buffer=&dest[y1+(x1 & 0xF8)*8];

	// The first pixel mask
	bit=pow2[x1 & 0x7];
	
	// len = longer slope of the line
	if (dx>dy) len=dx; else len=dy;

	_asm

	ld de,#1 
	ld a,(_sy)
	cp #1
	jp z,sy_pos
	ld de,#-1 ; DE = Destination Y increment (+1 or -1)
sy_pos:
	// Self-modify the code to gain speed and spare registers
	ld a,(_dy)
	ld (dy_test+1),a
	ld (err_plus_dy+1),a
	ld a,(_dx)
	ld (dx_test+1),a
	ld (err_plus_dx+1),a
	ld a,(_color)
	ld (mask_place+1),a
	
	ld a,(_len)
	ld b,a // B = counter
	ld hl,(_buffer) // HL = offscreen buffer
	ld a,(_bit)
	ld c,a // C = bitmask
	ld a,(_err) // A = error term
	
	ld ix,#0
	add ix,sp 
	ld sp,#64 // SP = Destination X increment
	di

mainloop:
	
	ld a,(_err)
	add a,a // A=2*err
dy_test:
	add a,#0 // =a+=dy (modified here)
	bit #7,a
	jp nz,not_overflow_dy // if !(2*err>-dy)
	ld a,(_err)	
err_plus_dy:
	sub #0 // a-=dy (modified here)
	ld (_err),a
	rrc c
	jp nc,update_pixel
	add hl,sp	
update_pixel:
	ld a,c
	xor (hl) // we do this to prevent near pixels, problematic for the EOR filler
mask_place:
	and #0xFF // masked (modified here)
	ld (hl),a			
not_overflow_dy:

	ld a,(_err)
	add a,a // A = 2*err
dx_test:
	sub #0
	bit #7,a
	jp z,not_overflow_dx // if !(e2<dx)
	ld a,(_err)
err_plus_dx:
	add a,#0 // a+=dx
	ld (_err),a
	add hl,de
	
not_overflow_dx:
	dec b
	jp nz,mainloop

	ld sp,ix
	ei

	_endasm;
		
}

void vdp_eor_fill(unsigned char *src,unsigned columns) {
/* EOR filler
	INPUT:	src	The source of the fill
		columns	The number of columns to fill

   The fill is made directly into the video screen.

   An EOR fill is a polygon filler ideal for slow computers.
   The fill travels through the screen, changing the fill "state" 
   when it encounters a pixel. As the MSX video memory is 
   organized as planar bytes (1 byte = 8 pixels), this routine can be 
   a highly efficient way of filling large areas.*/
 
    src;columns;
	_asm
	// load parameters
	ld l,4(ix) 
    	ld h,5(ix) // HL = filled area
    	ld e,6(ix) // E = number of columns
	
	0$:
	ld a,#0
	ld d,#16
	1$:
	// The main loop is simple
	xor (hl)
	out (0x98),a
	inc hl
	// We unravel it a bit to gain speed 
	xor (hl)
	out (0x98),a
	inc hl
	xor (hl)
	out (0x98),a
	inc hl
	xor (hl)
	out (0x98),a
	inc hl
	
	dec d
	jp nz,1$
	dec e
	jp nz,0$
	_endasm;
}

void my_isr(void) interrupt { 
	/* The interruption handler, does nothing but increments the timer.*/
	master_frame++;
	DI;
	READ_VDP_STATUS;
	EI;
}


int main(char **argv,int argc)
{
    	char c;
    	int k,i,j,x1,y1,x2,y2;
    	char base1,base2,plus,angles;
    	int nof_frames;
    	int fps;
	
    	printf("Polygon tests by Antti Silvast (antti.silvast@iki.fi), 2011, 2012. Use Q to quit.");

	/* precalculate a sin/5 table*/
    	for (i=0; i<256; i++)
		sini5[i]=sini[i]/5;
	
    	DI; // Disable Interrupts
    	screen(2); // graphics screen mode

	/* screen setup */
    	vdp_register(VDP_MODE1,MODE1_IE+MODE1_VRAM+MODE1_BLANK+
                 MODE1_SPRITE_SZ +MODE1_SPRITE_MAG );
    	vdp_register(VDP_COLOR,BLACK);

    	EI; // Enable Interrupts
    
	// Empty the hidden screen buffer
    	mem_set(line_offscreen_buffer,0x0,32*64);

	/* the interruption routine is installed here.*/
  	install_isr(my_isr);
 
	/* set the name table.*/
	set_name_table(2);
	
	/* Set the colors (black on white) */
	vdp_address(0x2000);
	vdp_set(0xF1,256*8*3);
	
	nof_frames=0;
	
    	while(1) {
		// reading the keyboard
		keyboard_line = (keyboard_line & 0xF0) | 4; // select line 4
		c = keyboard_column_r;
		if (!(c & 64)) {
			break; // user pressed Q
		}

		// empty offscreen buffer
		mem_set(&line_offscreen_buffer[0],0x0,32*64);
		
		// Draw three test polygons
		angles=4; k=1;
		for (j=48; j<256; j+=80, angles+=0, k+=1) {
		
			plus=256/(angles);
			base1=master_frame+k*33;
			base2=master_frame+plus+k*33;
			for (i=0; i<angles; i++, base1+=plus, base2+=plus) {
				/* every other polygon is different color */
				if ((k&1)==1) c=0xFF; else c=85;
				x1=j+sini5[(base1+64) & 0xFF];
				y1=32+sini5[(base1) & 0xFF];
				x2=j+sini5[(base2+64) & 0xFF];
				y2=32+sini5[(base2) & 0xFF];
				line(line_offscreen_buffer,x1,y1,x2,y2,c);
			}
		}
		
		/* Wait for the screen blank (when the video beam is up) */
		waitVB();

		/* Refresh to screen... 
		
		First, just the lines:*/
		vdp_address(0x0);
		vdp_bigcopy(line_offscreen_buffer,64);
		/* Then, with EOR fill: */
		vdp_address(0x800);
		vdp_eor_fill(line_offscreen_buffer,32);
		nof_frames++;
    	}

	/* Return to the MSX-DOS, print the Frames Per Second (FPS).*/

    	uninstall_isr();
    	screen(0);

    	fps=50*nof_frames/master_frame;
    	printf("FPS %d\n",fps);

    	return 0;
}
