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

// this array stores the values sin/4 (multiplications and divisions do not exist on the Z80 processor and are thus slow). 
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


void set_name_table() {
/* Set the name table

   Name tables are the "letters" of the MSX video memory, 
   which tell where each graphics character (0..256) lies on the screen. 

  The routine sets up two name tables to be used as alternating hidden buffers. 
*/

	_asm

	ld l,#2

	// the first page name table

	ld a,#00
	out (0x99),a
	ld a,#0x18+0x40	
	out (0x99),a
	
	ld c,#0

make_name_table:
	ld h,c // H stores the "blank" border character

	ld b,#24
1$:
	ld a,h
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a

	ld a,c
	ld d,#16
2$:	
	out (0x98),a
	inc a
	dec d
	jp nz,2$
		
	add a,#16
	ld c,a

	ld a,h
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a

	dec b
	jp nz,1$

	// the second page name table
	ld a,#00
	out (0x99),a
	ld a,#0x1C+0x40	
	out (0x99),a

	ld c,#16

	dec l
	jp nz,make_name_table
	
	_endasm;
}

void set_color_table(void) {
/* Sets up the MSX color table to a preset mask. */

	vdp_address(0x2000); // this is where the color table is
_asm
		
		ld b,#24
outer_loop:
		ld e,#32
inner_loop:
		ld a,#0xC1 // green on black
		out (0x98),a
		out (0x98),a
		out (0x98),a
		out (0x98),a
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
   Uses an 8-bit register to calculate the Bresenham error term. 
   Delta_x (x2-x1) and Delta_y (y2-y1) are max. 64!

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

	// set the color by self-modifying the main loop code
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

	// calculate and store dy=abs(y2-y1)
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
	// and the line length len=max(dx,dy)
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
	nop // for col==1. if col==0: CPL	
	or b // for col==1. if col==0: AND B
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

	// if e2<dx, then the Y coordinate grows (or decreases if y1>y2)
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

void erase_page(int start_address) {
/* Erases the screen in chunks of 128 bytes.  
	INPUT: 	start_address		start erasing here 
*/
	start_address;
	_asm
	// load parameters
	ld l,4(ix) // 
    	ld h,5(ix) // HL=start_address

	ld a,h
	or #0x40
	ld h,a // write address bit set

	ld c,#0x99 // C = MSX video memory address port

	ld e,#24 // E = number of character-rows
repeat_erase_page:	
	// define new write address
	out (c),l
	out (c),h

// erase 128 bytes
	sub a
	ld b,#4
fill_128:
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
	jp nz,fill_128
	
	inc h // skip the gap
	dec e
	jp nz,repeat_erase_page

	_endasm;
}

void erase_first_patterns(int start_address) {
/* Erases the first character pattern of each part of the video memory 

	INPUT:	start_address		where to start the erasing

*/
	start_address;
	_asm
	// load parameters
	ld l,4(ix) // 
    	ld h,5(ix) // HL=start_address

	ld a,h
	or #0x40
	ld h,a // write address bit set

	ld c,#0x99
	ld b,#3
first_erase_loop:
	out (c),l
	out (c),h
	sub a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	out (0x98),a
	ld a,h
	add a,#0x08
	ld h,a
	dec b
	jp nz,first_erase_loop
	_endasm;
}

void draw_polygon(unsigned char X, unsigned char Y, unsigned char angles, unsigned char plus, unsigned char rot) {
/* draws a closed line polygon.
   INPUT:	X,Y		screen coordinates of the polygon
		angles		the angles of the polygon
		plus		the step between the vertices
		rot		the starting rotation (0..255)

*/
	unsigned char j;
	unsigned char ind1,ind2,ind3,ind4;
	unsigned char x1,y1,x2,y2;

	ind1=rot+64;
	ind2=rot;
	ind3=rot+plus+64;
	ind4=rot+plus;
	for (j=0; j<angles; j++) {				
		x1=sini2[ind1]+X;
		y1=sini2[ind2]+Y;
		x2=sini2[ind3]+X;
		y2=sini2[ind4]+Y;
		line(x1,y1,x2,y2,1);
		ind1+=plus;
		ind2+=plus;
		ind3+=plus;
		ind4+=plus;
	}

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
    	long int nof_frames;
	int frame_count;
    	int fps;
	int i,X,Y;
	unsigned char frame1,frame2,frame3,ind1,ind2;
	unsigned char xpos1;
	unsigned char nt_pos;
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
	set_name_table();
	
	/* Set the colors */
	waitVB();
	set_color_table();
	
	nof_frames=frame_count=0;

    	while(1) {
		// reading the keyboard
		keyboard_line = (keyboard_line & 0xF0) | 4; // select line 4
		c = keyboard_column_r;
		if (!(c & 64)) {
			break; // user pressed Q
		}
		
		// make the page switch
		nt_pos = 0x1-frame_count & 0x1;
		xpos1 = (frame_count & 0x1)*128;

		// set name table position
		vdp_register(VDP_NAME_T,0x6+nt_pos);

		// erase previous buffer
		erase_page(xpos1);

		/* Wait for the screen blank (when the video beam is up) */
		// waitVB();
		
		/* draw three test polygons */

		frame1=master_frame; 
		frame2=2*frame1;
		frame3=3*frame1;

		// a square
		ind1=frame2+64; ind2=frame1;
		X=xpos1+sini2[ind1]+64; Y=sini2[ind2]*2+96;
		draw_polygon(X,Y,4,64,master_frame);
		// a pentagon
		ind1=frame2+20; ind2=frame3+80;
		X=xpos1+sini2[ind1]+64; Y=sini2[ind2]*2+96;
		draw_polygon(X,Y,5,51,master_frame);
		// a hexagon
		ind1=frame2+4; ind2=frame3+8;		
		X=xpos1+sini2[ind1]+64; Y=sini2[ind2]*2+96;
		draw_polygon(X,Y,6,43,master_frame);

		// we need a "blank" character in the name table
		// thus, erase the first patterns...
		// may cause disappearing artefacts because
		// the lines have already been drawn! 
		erase_first_patterns(xpos1);

		nof_frames++;
		frame_count++;
    	}

	/* Return to the MSX-DOS, print the Frames Per Second (FPS).*/

    	uninstall_isr();
    	screen(0);

    	fps=50*nof_frames/master_frame;
    	printf("FPS %d\n",fps);
    	return 0;
}
