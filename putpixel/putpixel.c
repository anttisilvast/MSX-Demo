
/* 

 A direct-to-screen putpixel routine and test for MSX by Antti Silvast, 2012.

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

void set_color_table(void) {
/* Sets up the MSX color table to a preset mask, sliding from white to red. */

	vdp_address(0x2000); // this is where the color table is
_asm
		
		ld b,#12
outer_loop:
		ld e,#0
inner_loop:
		ld a,#0xF1 // white on black
		out (0x98),a
		dec e
		jp nz,inner_loop

		ld b,#12
outer_loop2:
		ld e,#0
inner_loop2:
		ld a,#0x91 // bright red on black
		out (0x98),a
		dec e
		jp nz,inner_loop2
		dec b
		jp nz,outer_loop2

		ld b,#6
outer_loop3:
		ld e,#0
inner_loop3:
		ld a,#0x81 // red on black
		out (0x98),a
		dec e
		jp nz,inner_loop3
		dec b
		jp nz,outer_loop3

		ld b,#6
outer_loop4:
		ld e,#0
inner_loop4:
		ld a,#0x61 // dark red on black
		out (0x98),a
		dec e
		jp nz,inner_loop4
		dec b
		jp nz,outer_loop4

		_endasm;

}


/* These variables store the output of the calculations of the putpixel and
   erasepixel routines.*/
static int draw_dest;
static unsigned char draw_mask;

// powers of two (2^7...2^0)
unsigned char pow2[8]={128,64,32,16,8,4,2,1};

void put_pixel(unsigned char x, unsigned char y) {
/* Puts a pixel directly into the MSX pattern table. No clipping. 
	INPUT: 	x,y		coordinates

	OUTPUT:	draw_dest	the destination offset (for future reference)
		draw_mask	the pixel mask (for future reference)
*/
	x,y;
	_asm
	// load parameters
	ld l,4(ix) // L=x
    	ld h,5(ix) // H=y

	// draw to address dest = (y & 0x7 + x & 0xF8) + (y >> 3)*256
	ld a,l
	and #0xF8
	ld c,a 	
	ld a,h
	and #0x7
	add a,c
	ld c,a	
	ld b,h
	srl b
	srl b
	srl b ; BC = dest
	
	// store destination for possible future pixel erasing
	ld (_draw_dest),bc		

	// read what is already on the screen
	
	// set read address
	ld a,c
	out (0x99),a
	ld a,b
	out (0x99),a
	in a,(0x98)
	ld e,a // read

	// determine the new pixel mask
	ld a,l
        and #0x7

        ld hl,#_pow2
        add a,l
        ld l,a
        
        // write address
	ld a,c
	out (0x99),a
	ld a,b
	or #0x40
	out (0x99),a

	// read mask
	ld a,(hl)
	// also store the mask for possible future pixel erasing
	ld (_draw_mask),a
	// put pixel
	or e
	out (0x98),a
	_endasm;
}

void erase_pixel(unsigned char x, unsigned char y) {
/* Erase a pixel directly from the MSX pattern table. No clipping. 
	INPUT: 	x,y		coordinates

	OUTPUT:	draw_dest	the destination offset (for future reference)
		draw_mask	the pixel mask (for future reference)
*/

	x,y;
	_asm
	// load parameters
	ld l,4(ix) // L=x
    	ld h,5(ix) // H=y

	// draw to address dest = (y & 0x7 + x & 0xF8) + (y >> 3)*256
	ld a,l
	and #0xF8
	ld c,a 	
	ld a,h
	and #0x7
	add a,c
	ld c,a	
	ld b,h
	srl b
	srl b
	srl b ; BC = dest
	
	// store destination for future pixel erasing
	ld (_draw_dest),bc		

	// read what is already on the screen
	
	// set read address
	ld a,c
	out (0x99),a
	ld a,b
	out (0x99),a
	in a,(0x98)
	ld e,a // read

	// determine the new pixel mask
	ld a,l
        and #0x7

        ld hl,#_pow2
        add a,l
        ld l,a
        
        // write address
	ld a,c
	out (0x99),a
	ld a,b
	or #0x40
	out (0x99),a

	// read mask
	ld a,(hl)
	// also store the mask for future pixel erasing
	ld (_draw_mask),a
	// erase pixel
	cpl // negate to erase
	and e
	out (0x98),a

	_endasm;
}

void erase_vram(int dest, unsigned char mask) {
/* This is a fast routine for erasing a specific mask from a defined video memory address.
   Not in use right now, but kept here for future reference.

	INPUT:	dest	the destination video memory (starting from 0)
		mask	the pixel mask (e.g. 1 = pixel on the right, 128 = pixel on the left)
	*/

	dest,mask;
	_asm
	// load parameters
	ld c,4(ix) // 
    	ld b,5(ix) // BC=dest
	ld a,6(ix) // A=mask

	cpl // negate the mask as we are going to AND with it.
	ld e,a // e=!mask

	// set read address
	ld a,c
	out (0x99),a
	ld a,b
	out (0x99),a
	// read what is on the screen
	in a,(0x98)
	
	// erase by ANDing the mask
	and e 
	ld e,a

	// set write address
	ld a,c
	out (0x99),a
	ld a,b
	or #0x40
	out (0x99),a

        // put the new pixel to the dest address
	ld a,e
	out (0x98),a

	_endasm;
}
void my_isr(void) interrupt { 
	/* The interruption handler, does nothing but increments the timer.*/
	master_frame++;
	DI;
	READ_VDP_STATUS;
	EI;
}


/* the number of plots */
#define N 25

/* the delay for the tail of the plots */
#define DELAY 8

int main(char **argv,int argc)
{
    	char c;
    	int nof_frames;
    	int fps;
	int i;

	int base1,base2;
	int X,Y;
	char tilt;
    	printf("Putpixel tests by Antti Silvast (antti.silvast@iki.fi), 2012. Use Q to quit.");

	// calculate the other sin table (sin*3/4)
	for (i=0; i<256; i++) sini2[i]=(sini[i]*3/4);

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
		
		/* Draw some sinus patterns with the put_pixel routine */	
		tilt=0;
		base1=master_frame;
		base2=master_frame+64;
		for (i=0; i<N; i++) {

			// The draw part. Note that the figure also scrolls and is tilted to the left. 
			X=sini[base1 & 0xFF]+128+tilt+master_frame;
			Y=sini2[base2 & 0xFF]+96;
			put_pixel(X,Y);

			// The erase part. Note that this figure scrolls to the left and is tilted as well. 
			// To erase previous figures, everything is offset by -DELAY on the time axis.
			// The larger the DELAY, the longer the trail the pixels leave. 
			X=sini[(base1-DELAY) & 0xFF]+128+tilt+master_frame-DELAY;
			Y=sini2[(base2-DELAY) & 0xFF]+96;
			erase_pixel(X,Y);

			base1+=16;
			base2+=17;
			tilt+=7;
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
