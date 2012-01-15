/* 

 A direct-to-screen line draw and test for MSX by Antti Silvast, 2012.

 Compiling:
 - Type make at the command line. 
 
*/


#include <stdio.h>
#include <msxlib.h>
#include <interrupt.h>
#include <math.h>

#include "sin_table.inc"

// this array stores the values sin*3/4 (divisions do not exist on the Z80 processor and are thus slow). 
signed char sini2[256];

// constants for the keyboard reader
sfr at 0xAA keyboard_line;
sfr at 0xA9 keyboard_column_r;

// the time elapsed measured in 1/50 s
int master_frame=0;

void Dummy_function()
// This Dummy function is here towards mitigating what seems like a compiler or a library bug.
// The first routine of the function gets overwritten by someone, so we define this to 
// prevent the crash. 
{
	char tmp[256];
	tmp[0]=0;	

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
		1$:
		ld b,#0
		ld d,#8
		2$:
		ld e,#32
		3$:
		ld a,b
		out (0x98),a
		inc b
		dec e
		jp nz,3$
		dec d
		jp nz,2$
		dec c
		jp nz,1$
		_endasm;
		
	} else {
		
		_asm
		// Set the pseudolinear top-to bottom state
		ld d,#3
		4$:

		ld b,#0 // B: character number
		ld c,#8
		5$:
		push bc
		ld e,#32
		6$:
		ld a,b
		out (0x98),a
		add a,#8
		ld b,a
		dec e
		jp nz,6$

		pop bc
		inc b
		dec c
		jp nz,5$
		dec d
		jp nz,4$
		_endasm;
		
	}
}

void set_color_table(void) {
/* Sets up the MSX color table to a preset mask, sliding from white to red. */

	vdp_address(0x2000); // this is where the color table is
_asm
		
		ld b,#24
outer_loop:
		ld e,#32
inner_loop:
		ld a,#0xF1 
		out (0x98),a
		out (0x98),a
		out (0x98),a
		out (0x98),a
		ld a,#0xE1 
		out (0x98),a
		out (0x98),a
		out (0x98),a
		out (0x98),a
		dec e
		jp nz,inner_loop
		dec b
		jp nz,outer_loop


		_endasm;

}


// powers of two (2^7...2^0)
unsigned char pow2[8]={128,64,32,16,8,4,2,1};

void line(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2, unsigned char col) {
/* Draws a line directly into the MSX pattern table. No clipping. 
	INPUT: 	x1,y1		start coordinates
		x2,y2		end coordinates
		col		color (1=draw, 0=erase)

	 USED Z80 REGISTERS:
	   	B 		dx = abs(x2-x1)
	   	C 		dy = abs(y2-y1)
	   	D 		line length = max(dx,dy)
	   	E 		Bresenham error = dx-dy (initially)
	  	DE' 		video memory write address
	   	B' 		the mask for 1 byte (8 pixels)
	   	L' 		the mask for current pixel (1 bit)
	   	C' 		0x99 (MSX video address port)
	

*/
	x1,y1,x2,y2,col;
	_asm
	// load parameters
	ld l,4(ix) // L=x1
    	ld h,5(ix) // H=y1
	ld e,6(ix) // E=x2
	ld d,7(ix) // D=y2
	ld c,8(ix) // C=col

	// apply the color by self-modifying the main loop code
	ld a,c
	cp #1
	jp z,color_draw
color_erase:
	ld a,#0x2F 		// CPL
	ld (putpixel_code),a
	ld a,#0xA0 		// AND B
	ld (putpixel_code+1),a
	jp color_end	
color_draw:
	ld a,#0 		// NOP
	ld (putpixel_code),a
	ld a,#0xB0 		// OR B
	ld (putpixel_code+1),a
color_end:
	
	// arrange so that x2 is always larger than x1
	ld a,e
	sub l // A = x2-x1
	jp nc,x2_lt_x1
x1_lt_x2:
	ex de,hl // swap (x1,y1) <-> (x2,y2)
	neg // A=-A 
x2_lt_x1:
	// store A = dx = abs(x2-x1)
	ld b,a // B=dx

	// calculate and store dy=abs(y2-Y1)
	ld a,d
	sub h // A = y2-y1
	jp nc,y2_lt_y1
y1_lt_y2:
	neg // negate the negative dy value
	ld c,a // C=dy

	// now, to gain speed and spare registers, we self-modify
	// the code for the cases that y2>y1 or y1>y2 respectively
	// if y1>y2:
	ld a,#0 		// CP #0
	ld (y_cp_code+1),a
	ld a,#0x15 		// DEC D
	ld (y_inc_code),a
	ld a,#7 		// OR #7	
	ld (y_or_code+1),a
	ld a,#0x1D 		// DEC E
	ld (y_in_char),a

	jp y_cmp_done
y2_lt_y1:
	ld c,a // C=dy

	// if y2>y1:
	ld a,#7 		// CP #7
	ld (y_cp_code+1),a
	ld a,#0x14 		// INC D
	ld (y_inc_code),a
	ld a,#0			// OR #0
	ld (y_or_code+1),a
	ld a,#0x1C 		// INC E
	ld (y_in_char),a
y_cmp_done:
	
	// calculate the Bresenham error term err=dx-dy
	// and the line length len=max(dx,dy)+1
	ld a,b
	sub c
	jp c,dy_lt_dx
dx_lt_dy: 
	ld d,b // D = len
	jp bres_init_done
dy_lt_dx:
	ld d,c // D = len
bres_init_done:
	ld e,a // E=err

	push hl // HL has (x1,y1) which we need as the starting coordinate.
	exx // BC <-> BC', DE <-> DE', HL <-> HL'
	pop hl // L = x1, H = y1
 	
	ld c,#0x99 // video memory address port

	// calculate initial video address = (y1 & 0x7 + x1 & 0xF8) + (y1 >> 3)*256
	ld a,l
	and #0xF8
	ld e,a 	
	ld a,h
	and #0x7
	add a,e
	ld e,a	
	ld d,h
	srl d
	srl d
	srl d ; DE = dest
	
	// read what is already on the screen on this spot

	// set video memory read address
	out (c),e
	out (c),d
	in a,(0x98)
	ld b,a // B = screen content

	// determine the mask for the new pixel
	ld a,l
        and #0x7
        ld hl,#_pow2
        add a,l
        ld l,a
	ld a,(hl)
	ld l,a // L = pixel mask
        
        // set video memory write address
	out (c),e
	ld a,d
	or #0x40
	out (0x99),a

	exx // go back to main registers

	// loop for D times
line_main_loop:
// put pixel to mask on X,Y
	exx
	ld a,l
putpixel_code:
	nop // if col=0: CPL	
	or b // if col=0: AND B
	ld b,a
	exx

	ld h,#0 // update the video memory?

	ld a,e
	add a,a // A = e2=2*err	
	ld l,a // L spares e2

	// X Bresenham error term check

	// if e2>-dy, then the X coordinate grows	
	add a,c // A = e2+dy
	jp m,x_same
x_plus:	
	// update the error term
	ld a,e
	sub c
	ld e,a // err=err-dy	

	exx

	// roll the pixel bit to the right (because X+=1)
	rrc l
	// if that roll overflows, also update the address
	jp nc,x_plus_done	
	// calculate the new dest address
	ld a,e
	add a,#0x8 // move right to the next character (8 bytes)
	ld e,a
	exx
	ld h,#1 // set flag to update the video memory
	jp x_same
x_plus_done:
	exx
x_same:	

	ld a,l // A=e2 from before
	
	// Y Bresenham error term check

	// if e2<dx, then the Y coordinate grows
	sub b // A = e2-dx
 	jp p,y_same
y_plus:
	// update the error term
	ld a,e
	add b
	ld e,a // err=err+dx

	ld h,#1 // set flag to update the video memory

	exx
	// calculate the new dest address

	// If y within the char is 7 (or 0), we need to move to 
	// another character.
	// Otherwise we just move inside the character. 
	ld a,e
	and #0x7
y_cp_code:
	cp #0x7	// for (y2>y1). if (y1>y2) then: CP #0
	jp nz,y_in_char
y_inc_code:
	inc d // for (y2>y1). if (y1>y2) then: DEC D
	ld a,e
	and #0xF8
y_or_code:
	or #0 // for (y2>y1). if (y1>y2) then: OR #7
	ld e,a
	jp y_plus_done
y_in_char:
	inc e // for (y2>y1). if (y1>y2) then: DEC E
y_plus_done:	
	exx
y_same:

	// check whether the bitmap needs to be updated
	ld a,h
	cp #0
	jp z,skip_update
update_pixel:
	exx
	// write the bitmap to the video memory
	ld a,b
	out (0x98),a	

	// define new read address
	out (c),e
	out (c),d
	// read new underlying pixel
	in a,(0x98)
	ld b,a 

	// define new write address
	out (c),e
	ld a,d
	or #0x40
	out (0x99),a
	exx
skip_update:

	dec d
	jp nz,line_main_loop
	
	_endasm;
}

void erase_step(int start_address, int step, unsigned char n) {
/* Erases the screen in chunks of 256 bytes for speed gain.  
	INPUT: 	start_address		start erasing here 
		step			the interval between the 256 byte chunks
		n			the number of erases to make
*/
	start_address,step,n;
	_asm
	// load parameters
	ld l,4(ix) // 
    	ld h,5(ix) // HL=start_address
	ld e,6(ix) // 
	ld d,7(ix) // DE=step
	ld c,8(ix) // C=n

	ld a,h
	or #0x40
	ld h,a // write address bit set

repeat_erase:	
	// define new write address
	ld a,l
	out (0x99),a
	ld a,h
	out (0x99),a

// erase 256 bytes
	sub a
	ld b,#8
fill_256:
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a

	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a

	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a

	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a

	dec b
	jp nz,fill_256
	
	add hl,de // fill the gap
	dec c
	jp nz,repeat_erase

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
    	int nof_frames;
    	int fps;
	int i;
	unsigned char j;
	int pos;
	unsigned char base1;
	int x1,y1,x2,y2,X,Y;
    	printf("Line tests by Antti Silvast (antti.silvast@iki.fi), 2012. Use Q to quit.");

	// calculate the other sin table (sin/4)
	for (i=0; i<256; i++) sini2[i]=(sini[i]/4);

    	DI; // Disable Interrupts
    	screen(2); // graphics screen mode

	/* screen setup */
    	vdp_register(VDP_MODE1,MODE1_IE+MODE1_VRAM+MODE1_BLANK+
                 MODE1_SPRITE_SZ +MODE1_SPRITE_MAG );
    	vdp_register(VDP_COLOR,BLACK);

    	EI; // Enable Interrupts
    
	/* the interruption routine is installed here.*/
  	install_isr(my_isr);
 
	/* set the name table.*/
	waitVB();
	set_name_table(1);
	
	/* Set the colors */
	waitVB();
	set_color_table();
	
	nof_frames=0;

    	while(1) {
		// reading the keyboard
		keyboard_line = (keyboard_line & 0xF0) | 4; // select line 4
		c = keyboard_column_r;
		if (!(c & 64)) {
			break; // user pressed Q
		}
		
		/* Wait for the screen blank (when the video beam is up) */
		waitVB();

		if ((master_frame % 1000)>=500) {
			/* erase the whole screen in two big parts */
			pos=nof_frames & 0x1;
			erase_step(pos*3072,256,12);
		} else {
			/* clear the screen in three intertwined parts */
			pos=nof_frames % 3;
			erase_step(pos*256,768,8);
		}
		/* other screen clearing options: */
		/* clear the screen in two intertwined parts */
		//pos=nof_frames & 0x1;
		//erase_step(pos*256,512,12);	
		/* clear the screen in four intertwined parts */
		//pos=nof_frames & 0x3;
		//erase_step(pos*256,1024,6);

		/* draw three test polygons */

		// a square
		#define PLUS 64
		X=sini2[(master_frame*2+64) & 0xFF]*2+128; Y=sini2[(master_frame) & 0xFF]*2+96;
		base1=master_frame;		
		for (j=0; j<4; j++) {				
				x1=sini2[(base1+64) & 0xFF]+X;
				y1=sini2[(base1) & 0xFF]+Y;
				x2=sini2[(base1+PLUS+64) & 0xFF]+X;
				y2=sini2[(base1+PLUS) & 0xFF]+Y;
				line(x1,y1,x2,y2,1);
				base1+=PLUS;
		}

		// a pentagon
		#define PLUS2 51
		X=sini2[(master_frame*2+64) & 0xFF]*2+128; Y=sini2[(master_frame*3+80) & 0xFF]*2+96;
		base1=master_frame;		
		for (j=0; j<5; j++) {				
				x1=sini2[(base1+64) & 0xFF]+X;
				y1=sini2[(base1) & 0xFF]+Y;
				x2=sini2[(base1+PLUS2+64) & 0xFF]+X;
				y2=sini2[(base1+PLUS2) & 0xFF]+Y;
				line(x1,y1,x2,y2,1);
				base1+=PLUS2;
		}

		// a hegaon
		#define PLUS3 42
		X=sini2[(master_frame*2+4) & 0xFF]*2+128; Y=sini2[(master_frame*3+8) & 0xFF]*2+96;
		base1=master_frame;		
		for (j=0; j<6; j++) {				
				x1=sini2[(base1+64) & 0xFF]+X;
				y1=sini2[(base1) & 0xFF]+Y;
				x2=sini2[(base1+PLUS3+64) & 0xFF]+X;
				y2=sini2[(base1+PLUS3) & 0xFF]+Y;
				line(x1,y1,x2,y2,1);
				base1+=PLUS3;
		}

		nof_frames++;
    	}

	/* Return to the MSX-DOS, print the Frames Per Second (FPS).*/

    	uninstall_isr();
    	screen(0);

    	fps=50*nof_frames/master_frame;
    	printf("FPS %d\n",fps);
    	return 0;
}
