/**
 *
 * Phantom OS
 *
 * Copyright (C) 2005-2010 Dmitry Zavalishin, dz@dz.ru
 *
 * Kernel ready: yes
 *
 *
**/

#include <drv_video_screen.h>
#include <video/screen.h>
#include <assert.h>
#include <sys/types.h>


// Find pointer to the line on screen. As addresses go from left to right in
// both screen and sys coords, this is the only strange calculation here.

// Zeroth line will have index of (scr y size - 1), right?

#define DRV_VIDEO_REVERSE_LINESTART(ypos) ( (video_drv->xsize * ((video_drv->ysize -1) - ypos) ) * bit_mover_byte_step + video_drv->screen)


#define DRV_VIDEO_FORWARD_LINESTART(ypos) ( (video_drv->xsize * ypos) * bit_mover_byte_step + video_drv->screen)


// movers. default to 24bpp
// void rgba2rgba_zbmove( struct rgba_t *dest, const struct rgba_t *src, zbuf_t *zb, int nelem, zbuf_t zpos )
static void (*bit_zbmover_to_screen)( void *dest, const struct rgba_t *src, zbuf_t *zb, int nelem, zbuf_t zpos ) = (void *)rgba2rgb_zbmove;
static void (*bit_mover_to_screen)( void *dest, const struct rgba_t *src, int nelem ) = (void *)rgba2rgb_move;
static void (*bit_mover_from_screen)( struct rgba_t *dest, void *src, int nelem ) = (void *)rgb2rgba_move;
static int      bit_mover_byte_step = 3;


void switch_screen_bitblt_to_32bpp( int use32bpp )
{
    if(use32bpp)
    {
        bit_zbmover_to_screen = (void *)rgba2rgba_zbmove;
        bit_mover_to_screen   = (void *)rgba2rgba_move;
        bit_mover_from_screen = (void *)rgba2rgba_24_move;

        bit_mover_byte_step = 4;
        video_drv->bpp = 32;
    }
    else
    {
        bit_zbmover_to_screen = (void *)rgba2rgb_zbmove;
        bit_mover_to_screen   = (void *)rgba2rgb_move;
        bit_mover_from_screen = (void *)rgb2rgba_move;

        bit_mover_byte_step = 3;
        video_drv->bpp = 24;
    }
}

/**
 *
 * From buffer to screen.
 *
 * Magic is that screen (0,0) is top left and window (0,0)
 * is bottom left.
 *
 **/

void drv_video_bitblt_worker(const struct rgba_t *from, int xpos, int ypos, int xsize, int ysize, int reverse, zbuf_t zpos)
{
    //printf("bit blt pos (%d,%d) size (%d,%d)\n", xpos, ypos, xsize, ysize);
    assert(video_drv->screen != 0);

    // we can't do it here - mouse itself uses us!
    //drv_video_mouse_off();

    int xafter = xpos+xsize;
    int yafter = ypos+ysize;

    if(
       xafter <= 0 || yafter <= 0 ||
       xpos >= video_drv->xsize || ypos >= video_drv->ysize
      )
        return; // Totally clipped off

    // Where to start in source line
    int xshift = (xpos < 0) ? -xpos : 0;

    // Which source line to start from
    int yshift = (ypos < 0) ? -ypos : 0;

    if( yshift > 0 )
    {
        // This one is easy candy

        //printf("yshift = %d\n", yshift );
        from += xsize*yshift; // Just skip some lines;
        ysize -= yshift; // Less lines to go
        ypos += yshift;
        assert(ypos == 0);
        yshift = 0;
    }
    assert(yshift == 0);
    assert(xshift >= 0);


    //printf("xshift = %d\n", xshift );

    // xlen is how many pixels to move for each line
    int xlen = xsize;
    if( xafter > video_drv->xsize )
    {
        xlen -= (xafter - video_drv->xsize);
    }
    xlen -= xshift;
    assert(xlen > 0);
    assert(xlen <= xsize);
    assert(xlen <= video_drv->xsize);

    if( yafter > video_drv->ysize )
    {
        yafter = video_drv->ysize;
    }

    //char *lowest_line = ((drv_video_screen.ysize -1) - ypos) + drv_video_screen.screen;

    // We took it in account
    if(xpos < 0) xpos = 0;

    int sline = ypos;
    int wline = 0;


    //printf("xlen = %d, sline = %d yafter=%d \n", xlen, sline, yafter );

    if(reverse)
    {

        for( ; sline < yafter; sline++, wline++ )
        {
            // Screen start pos in line
            char *s_start = DRV_VIDEO_REVERSE_LINESTART(sline) + xpos*bit_mover_byte_step;
            // Window start pos in line
            const struct rgba_t *w_start = from + ((wline*xsize) + xshift);

            zbuf_t *zb = zbuf + ( (video_drv->xsize * ((video_drv->ysize-1) - sline)) + xpos);
            // ZBUF_TOP is a special value for mouse painting. XXX hack!
            if(zpos == ZBUF_TOP) bit_mover_to_screen( (void *)s_start, w_start, xlen );
            else bit_zbmover_to_screen( (void *)s_start, w_start, zb, xlen, zpos );
        }
    }
    else
    {
        for( ; sline < yafter; sline++, wline++ )
        {
            // Screen start pos in line
            char *s_start = DRV_VIDEO_FORWARD_LINESTART(sline) + xpos*bit_mover_byte_step;
            // Window start pos in line
            const struct rgba_t *w_start = from + ((wline*xsize) + xshift);

            //zbuf_t *zb = zbuf + ((wline*xsize) + xshift);
            zbuf_t *zb = zbuf + ( (video_drv->xsize * sline) + xpos);
            // ZBUF_TOP is a special value for mouse painting. XXX hack!
            if(zpos == ZBUF_TOP) bit_mover_to_screen( (void *)s_start, w_start, xlen );
            else bit_zbmover_to_screen( (void *)s_start, w_start, zb, xlen, zpos );
        }
    }

    drv_video_update();

    //drv_video_mouse_on();
}





/**
 *
 * From screen to buffer.
 *
 * Magic is that screen (0,0) is top left and window (0,0)
 * is bottom left.
 *
 **/

void drv_video_bitblt_reader(struct rgba_t *to, int xpos, int ypos, int xsize, int ysize, int reverse)
{
    //printf("bit blt pos (%d,%d) size (%d,%d)\n", xpos, ypos, xsize, ysize);
    assert(video_drv->screen != 0);

    int xafter = xpos+xsize;
    int yafter = ypos+ysize;

    if( xafter <= 0 || yafter <= 0
        || xpos >= video_drv->xsize || ypos >= video_drv->ysize )
        return; // Totally clipped off

    // Where to start in source line
    int xshift = (xpos < 0) ? -xpos : 0;

    // Which source line to start from
    int yshift = (ypos < 0) ? -ypos : 0;

    if( yshift > 0 )
    {
        // This one is easy candy

        //printf("yshift = %d\n", yshift );
        to += xsize*yshift; // Just skip some lines;
        ysize -= yshift; // Less lines to go
        ypos += yshift;
        assert(ypos == 0);
        yshift = 0;
    }
    assert(yshift == 0);
    assert(xshift >= 0);


    //printf("xshift = %d\n", xshift );

    // xlen is how many pixels to move for each line
    int xlen = xsize;
    if( xafter > video_drv->xsize )
    {
        xlen -= (xafter - video_drv->xsize);
    }
    xlen -= xshift;
    assert(xlen > 0);
    assert(xlen <= xsize);
    assert(xlen <= video_drv->xsize);

    if( yafter > video_drv->ysize )
    {
        yafter = video_drv->ysize;
    }

    //char *lowest_line = ((drv_video_screen.ysize -1) - ypos) + drv_video_screen.screen;


    int sline = ypos;
    int wline = 0;


    //printf("xlen = %d, sline = %d yafter=%d \n", xlen, sline, yafter );

    if(reverse)
    {

        for( ; sline < yafter; sline++, wline++ )
        {
            // Screen start pos in line
            char *s_start = DRV_VIDEO_REVERSE_LINESTART(sline) + xpos*bit_mover_byte_step;
            // Window start pos in line
            struct rgba_t *w_start = to + ((wline*xsize) + xshift);

            //rgba2rgb_move( (void *)s_start, w_start, xlen );
            bit_mover_from_screen( w_start, (void *)s_start, xlen );
        }
    }
    else
    {
        for( ; sline < yafter; sline++, wline++ )
        {
            // Screen start pos in line
            char *s_start = DRV_VIDEO_FORWARD_LINESTART(sline) + xpos*bit_mover_byte_step;
            // Window start pos in line
            struct rgba_t *w_start = to + ((wline*xsize) + xshift);

            //rgba2rgb_move( (void *)s_start, w_start, xlen );
            bit_mover_from_screen( w_start, (void *)s_start, xlen );
        }
    }

}


#if 0


void bitmap2bitmap(
                   struct rgba_t *dest, int destWidth, int destHeight, int destX, int destY,
                   const struct rgba_t *src, int srcWidth, int srcHeight, int srcX, int srcY,
                   int moveWidth, int moveHeight
                  )
{
    /*
     if( destX >= destWidth || destX+moveWidth <= 0)
     return;

     if( srcX >= srcWidth || srcX+moveWidth <= 0)
     return;
     */

    // now clip

    int leftExcessX = 0;

    if( destX+moveWidth >= destWidth )
        moveWidth = destWidth-destX;

    if( destX <= 0)
        leftExcessX = -destX;

    if( srcX+moveWidth >= srcWidth )
        moveWidth = srcWidth-srcX;

    if( srcX <= 0 && ((-srcX) < leftExcessX) )
        leftExcessX = -srcX;

    if(leftExcessX)
    {
        moveWidth -= leftExcessX;
        destX += leftExcessX;
        srcX += leftExcessX;
    }


    int leftExcessY = 0;

    if( destY+moveHeight >= destHeight )
        moveHeight = destHeight-destY;

    if( destY <= 0)
        leftExcessY = -destY;

    if( srcY+moveHeight >= srcHeight )
        moveHeight = srcHeight-srcY;

    if( srcY <= 0 && ((-srcY) < leftExcessY) )
        leftExcessY = -srcY;

    if(leftExcessY)
    {
        moveHeight -= leftExcessY;
        destY += leftExcessY;
        srcY += leftExcessY;
    }

    if( moveWidth <= 0 || moveHeight <= 0 )
        return;

    //printf("blit %d x %d", moveWidth, moveHeight );
    //int srcShift = srcX * sizeof(struct rgba_t);
    //int dstShift = destX* sizeof(struct rgba_t);
    //int srcLineStep = srcWidth * sizeof(struct rgba_t);
    //int dstLineStep = destWidth * sizeof(struct rgba_t);

    const struct rgba_t *srcPtr = src + srcY*srcWidth + srcX;
    struct rgba_t *dstPtr      = dest + destY*destWidth + destX;

    int hcnt;
    for(hcnt = moveHeight; hcnt; hcnt--)
    {
        rgba2rgba_move( dstPtr, srcPtr, moveWidth );
        dstPtr += destWidth;
        srcPtr += srcWidth;
    }
}



void rgba2rgb_zbmove( struct rgb_t *dest, const struct rgba_t *src, zbuf_t *zb, int nelem, zbuf_t zpos  )
{
    int *isrc = (int *)src;
    while(nelem-- > 0)
    {
#if 0
        // BUG don't update zbuf if alpha is zero?
        if( *zb > zpos ) { zb++; dest++; src++; continue; }
        *zb++ = zpos;

        if(src->a)
        {
            dest->r = src->r;
            dest->g = src->g;
            dest->b = src->b;
        }
        dest++;
        src++;
#else
        // BUG don't update zbuf if alpha is zero?

        if( !(*isrc>>24) || *zb > zpos ) { zb++; dest++; isrc++; continue; }
        *zb++ = zpos;
        //if( !(src->a) ) { dest++; src++; continue; }

        int w = *isrc++;

        dest->r = w >> 16;
        dest->g = w >> 8;
        dest->b = w >> 0;

        dest++;
#endif
    }
}

void rgb2rgba_zbmove( struct rgba_t *dest, const struct rgb_t *src, zbuf_t *zb, int nelem, zbuf_t zpos )
{
    while(nelem-- > 0)
    {
        if( *zb > zpos ) { zb++; dest++; src++; continue; }
        *zb++ = zpos;

        dest->a = 0xFF;

        dest->r = src->r;
        dest->g = src->g;
        dest->b = src->b;

        dest++;
        src++;
    }
}


void rgba2rgba_zbmove( struct rgba_t *dest, const struct rgba_t *src, zbuf_t *zb, int nelem, zbuf_t zpos )
{
    while(nelem-- > 0)
        if(src->a)
        {
            if( *zb > zpos ) { zb++; dest++; src++; continue; }
            *zb++ = zpos;

            *dest++ = *src++;
        }
        else
        {
            dest++;
            src++;
            zb++;
        }
}


void rgba2rgba_zbreplicate( struct rgba_t *dest, const struct rgba_t *src, zbuf_t *zb, int nelem, zbuf_t zpos )
{
    if(!src->a) return;
    while(nelem-- > 0)
    {
        if( *zb > zpos ) { zb++; dest++; continue; }
        *zb++ = zpos;
        *dest++ = *src;
    }
}



void int565_to_rgba_zbmove( struct rgba_t *dest, const short int *src, zbuf_t *zb, int nelem, zbuf_t zpos )
{
    while(nelem-- > 0)
    {
        if( *zb > zpos ) { zb++; dest++; src++; continue; }
        *zb++ = zpos;

        dest->a = 0xFF;

        dest->r = ((*src >>11) & 0x1F) << 3;
        dest->g = ((*src >>5)  & 0x3F) << 2;
        dest->b = (*src & 0x1F) << 3;

        dest++;
        src++;
    }
}


//#else

void rgba2rgb_move( struct rgb_t *dest, const struct rgba_t *src, int nelem )
{
    while(nelem-- > 0)
    {
        if(src->a)
        {
            dest->r = src->r;
            dest->g = src->g;
            dest->b = src->b;
        }
        dest++;
        src++;
    }
}

void rgb2rgba_move( struct rgba_t *dest, const struct rgb_t *src, int nelem )
{
    while(nelem-- > 0)
    {
        dest->a = 0xFF;

        dest->r = src->r;
        dest->g = src->g;
        dest->b = src->b;

        dest++;
        src++;
    }
}


void rgba2rgba_move( struct rgba_t *dest, const struct rgba_t *src, int nelem )
{
    while(nelem-- > 0)
        if(src->a)
        {
            *dest++ = *src++;
        }
        else
        {
            dest++;
            src++;
        }
}

void rgba2rgba_24_move( struct rgba_t *dest, const struct rgba_t *src, int nelem )
{
    while(nelem-- > 0)
    {
        if(src->a)
        {
            u_int32_t *d = (u_int32_t *)dest;
            u_int32_t *s= (u_int32_t *)src;
            *d = ((*s) & 0xFFFFFFu) | 0xFF000000u;
        }
        dest++;
        src++;
    }
}



void rgba2rgba_replicate( struct rgba_t *dest, const struct rgba_t *src, int nelem )
{
    if(!src->a) return;
    while(nelem-- > 0)
    {
        *dest++ = *src;
    }
}



void int565_to_rgba_move( struct rgba_t *dest, const short int *src, int nelem )
{
    while(nelem-- > 0)
    {
        dest->a = 0xFF;

        dest->r = ((*src >>11) & 0x1F) << 3;
        dest->g = ((*src >>5)  & 0x3F) << 2;
        dest->b = (*src & 0x1F) << 3;

        dest++;
        src++;
    }

}

#endif

















