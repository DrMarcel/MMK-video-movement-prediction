#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <memory.h>

/*
 * License:
 * CC BY 4.0
 *
 * Author: DrMarcel
 */

/*
 * Infos:
 * YUV, 4:2:0
 * CIF:352x288 -> Luminanz
 *     176x144 -> Chrominanz
 * Framerate:30Hz
 *
 * Dateiaufbau
 * Bild 1 YUV , Bild 2 YUV, Bild 3 YUV, ...
 * 1byte pro px
 *
*/



/* Constants
 */

static const unsigned int W = 352;
static const unsigned int H = 288;

static const unsigned int BLOCK_W = 16;
static const unsigned int BLOCK_H = 16;



/* Structure contains the YUV color data
 * of a Frame with the size WxH px.
 * The memory locaction of a specific pixel
 * can be reached with addr=y*W+x.
 */
typedef struct
{
    unsigned char* y;
    unsigned char* u;
    unsigned char* v;
} Frame;

/* Structure contains the YUV color data
 * of a Block with the size BLOCK_WxBLOCK_H px.
 * The memory locaction of a specific pixel
 * can be reached with addr=y*BLOCK_W+x.
 */
typedef struct
{
    unsigned char* y;
    unsigned char* u;
    unsigned char* v;
} Block;



/* Allocate memory for one Frame
 */
Frame* newFrame()
{
    Frame *frame = malloc(sizeof(Frame));

    frame->y = malloc(W*H);
    frame->u = malloc(W*H/4);
    frame->v = malloc(W*H/4);

    return frame;
}

/* Allocate memory for one Block
 */
Block* newBlock()
{
    Block *block = malloc(sizeof(Block));

    block->y = malloc(BLOCK_W*BLOCK_H);
    block->v = malloc(BLOCK_W*BLOCK_H/4);
    block->u = malloc(BLOCK_W*BLOCK_H/4);

    return block;
}

/* Free memory of one Frame
 */
void freeFrame(Frame* frame)
{
    free(frame->y);
    free(frame->u);
    free(frame->v);
    free(frame);
}

/* Free memory of one Block
 */
void freeBlock(Block* block)
{
    free(block->y);
    free(block->u);
    free(block->v);
    free(block);
}



/* Read the YUV data of a single Frame
 * out of a file. The file has to be opened
 * before and closed after the function call.
 * The function can be called multiple times
 * to read more than one Frame.
 */
Frame* readFrame(FILE *fp)
{
    Frame *frame = newFrame();

    fread(frame->y, sizeof(unsigned char), W*H, fp);
    fread(frame->u, sizeof(unsigned char), W*H/4, fp);
    fread(frame->v, sizeof(unsigned char), W*H/4, fp);

    return frame;
}

/* Write Y data of a Frame into a file.
 * The file has to be opened before and closed
 * after the function call.
 */
void writeFrameY(FILE *fp, Frame* frame)
{
    fwrite(frame->y, sizeof(unsigned char),W*H, fp);
}

/* Write U data of a Frame into a file.
 * The file has to be opened before and closed
 * after the function call.
 */
void writeFrameU(FILE *fp, Frame* frame)
{
    fwrite(frame->u, sizeof(unsigned char),W*H/4, fp);
}

/* Write V data of a Frame into a file.
 * The file has to be opened before and closed
 * after the function call.
 */
void writeFrameV(FILE *fp, Frame* frame)
{
    fwrite(frame->v, sizeof(unsigned char),W*H/4, fp);
}

/* Write a Frame into a file. The function can
 * be called multiple times to generate a *.yuv
 * conform file.
 * The file has to be opened before and closed
 * after the function call.
 */
void writeFrame(FILE *fp, Frame* frame)
{
    writeFrameY(fp, frame);
    writeFrameU(fp, frame);
    writeFrameV(fp, frame);
}



/* Returns a data array which contains the
 * difference d2 - d1 for each element of the
 * input arrays and the average difference.
 *
 * d1: Pointer to array 1
 * d2: Pointer to array 2
 * DW: 2d Array width
 * DW: 2d Array height
 * MSE: Return pointer for average difference
 */
unsigned char* getDifData(unsigned char* d1, unsigned char* d2, unsigned int DW, unsigned int DH, double* MSE)
{
    //Alloc output array
    unsigned char* ret = malloc(DW*DH);
    double MSEtmp=0;

    //Loop over all array elements
    for(unsigned int y=0; y<DH; y++)
    {
        for(unsigned int x=0; x<DW; x++)
        {
            //Calculate d2-d1
            int dif = (int)(d2[y*DW + x]) - (int)(d1[y*DW + x]);

            //Offset and overflow control
            int difoff = dif + 127;
            if(difoff > 255) difoff = 255;
            if(difoff < 0) difoff = 0;

            //Output data
            ret[y*DW + x] = (unsigned char)(difoff);

            //Sum squared difference over all elements
            MSEtmp += dif*dif;
        }
    }

    //Calculate and return MSE
    MSEtmp = MSEtmp / (W*H);
    if(MSE!=NULL) *MSE = MSEtmp;

    //Return array data
    return ret;
}

/* Returns difference between two Frames f2-f1
 * and the average difference MSE of the Luminance.
 */
Frame* getDifFrame(Frame* f1, Frame* f2, double* MSE)
{
    Frame *frame = newFrame();

    frame->y = getDifData(f1->y, f2->y, W, H, MSE);
    frame->u = getDifData(f1->u, f2->u, W/2, H/2, NULL);
    frame->v = getDifData(f1->v, f2->v, W/2, H/2, NULL);

    return frame;
}

/* Returns difference between two Blocks b2-b1
 * and the average difference MSE of the Luminance.
 */
Block* getDifBlock(Block* b1, Block* b2, double* MSE)
{
    Block *block = newBlock();

    block->y = getDifData(b1->y, b2->y, BLOCK_W, BLOCK_H, MSE);
    block->u = getDifData(b1->u, b2->u, BLOCK_W/2, BLOCK_H/2, NULL);
    block->v = getDifData(b1->v, b2->v, BLOCK_W/2, BLOCK_H/2, NULL);

    return block;
}



/* Return a single Block of a Frame at position (x,y)px.
 * The return value is NULL if the Block is out of
 * the Frame borders.
 */
Block* getBlock(Frame *frame, unsigned int x, unsigned int y)
{
    //Check if Block is within Frame
    if(x+BLOCK_W > W || y+BLOCK_H > H) return NULL;

    Block* block = newBlock();

    //Copy Block line by line
    //Line address in Block: iy*BLOCK_W
    //Line address plus Block offset in Frame: (y+iy)*W+x
    for(unsigned int iy=0; iy<BLOCK_H; iy++)
        memcpy(&block->y[iy*BLOCK_W], &frame->y[(y+iy)*W+x], BLOCK_W);

    return block;
}

/* Copy a single Block into a Frame at position (x,y)px.
 * Does nothing if Block is outside Frame border.
 */
void setBlock(Frame *frame, Block *block, unsigned int x, unsigned int y)
{
    //Check if Block is within Frame
    if(x+BLOCK_W > W || y+BLOCK_H > H) return;

    //Copy Block line by line
    //Line address in Block: iy*BLOCK_W
    //Line address plus Block offset in Frame: (y+iy)*W+x
    for(unsigned int iy=0; iy<BLOCK_H; iy++)
        memcpy(&frame->y[(y+iy)*W+x], &block->y[iy*BLOCK_W], BLOCK_W);

}

/* Get the Block from a source Frame which is most similar
 * to a block at a specific position in target Frame.
 * Searches in the source frame in the space (x+-area,y+-area).
 *
 * target: The frame that should be predicted
 * source: The source Frame from which the target should be predicted
 * x,y: Position of the Block in target Frame to predict
 * vectx,vecty: Pointer return the offset for the movement prediction
 * area: Space around (x,y) to search for a prediction Block
 */
Block* getPredictBlock(Frame* target, Frame* source, unsigned int x, unsigned int y, int* vectx, int* vecty, int area)
{
    Block *targetBlock = getBlock(target,x,y);

    //Save the data of the Block with the lowest MSE
    Block *lowestMSEBlock = NULL;
    double lowestMSE=0;
    int lowestMSEVectX=0;
    int lowestMSEVectY=0;

    //Loop over the search area
    for(int iy=-area; iy<=area; iy++)
    {
        for(int ix=-area; ix<=area; ix++)
        {
            //Calculate position of the Block to compare
            int compx=(int)x+ix;
            int compy=(int)y+iy;

            //Check if block is within source Frame
            if(compx>=0 && compy>=0 && (unsigned)compx+BLOCK_W<W && (unsigned)compy+BLOCK_H<H)
            {
                //Get Block from source to compare with target
                Block *compBlock = getBlock(source,(unsigned)compx,(unsigned)compy);

                //Calculate MSE between source and target Block
                double MSE;
                getDifBlock(compBlock,targetBlock,&MSE);

                //Check if MSE is lower than before
                //or no Block has been selected before
                if(MSE<lowestMSE || lowestMSEBlock==NULL)
                {
                    lowestMSE = MSE;
                    lowestMSEVectX=ix;
                    lowestMSEVectY=iy;
                    lowestMSEBlock = compBlock;
                }
            }
        }
    }

    //Return Block data
    *vectx = lowestMSEVectX;
    *vecty = lowestMSEVectY;
    return lowestMSEBlock;
}



int main()
{
    printf("\n*** Initialize ***\n");

    //Try to open input file
    printf("Open ../videodecoder/FOOTBALL_352x288_30_orig_01.yuv\n");
    FILE* fp_in = fopen("../videodecoder/FOOTBALL_352x288_30_orig_01.yuv", "rb");
    if(fp_in == NULL)
    {
        printf("File read error!\n");
        return 1;
    }

    //Read the first two frames
    printf("Read frame1\n");
    Frame* frame1  = readFrame(fp_in);
    printf("Read frame2\n");
    Frame* frame2  = readFrame(fp_in);

    //Close file
    printf("Close file\n");
    fclose(fp_in);



    // ### Methode 1 ###
    printf("\n*** Methode 1 ***\n");

    //Get dif frame and MSE
    printf("Calculate frame2 - frame1\n");
    double MSE = 0;
    Frame* out1 = getDifFrame(frame1, frame2, &MSE);

    //Output MSE
    printf("MSE: %f\n", MSE);

    //Try to open output file
    printf("Open file ../videodecoder/output_m1_error.yuv\n");
    FILE* fp_out = fopen("../videodecoder/output_m1_error.yuv", "wb");
    if(fp_out == NULL)
    {
        printf("File write error!");
        return 1;
    }

    //Output dif frame
    printf("Output frame data\n");
    writeFrameY(fp_out, out1);

    //Close file
    printf("Close file\n");
    fclose(fp_out);



    // ### Methode 2 ###
    printf("\n*** Methode 2 ***\n");

    //Generate prediction Frame
    Frame* predict = newFrame();

    //Loop over all Blocks in source Frame
    for(unsigned int by=0; by<H; by+=BLOCK_H)
    {
        for(unsigned int bx=0; bx<W; bx+=BLOCK_W)
        {
            //Find prediction for current Block
            printf("Predict block[%d,%d] : ", bx/BLOCK_W, by/BLOCK_H);
            int vx, vy;
            Block* ret = getPredictBlock(frame2,frame1,bx,by,&vx,&vy,16);
            printf("vector = (%d,%d)px\n", vx, vy);

            //Draw the prediction Block into prediction Frame
            setBlock(predict,ret,bx,by);
        }
    }

    //Get dif frame and MSE
    printf("Calculate predict_frame - frame2\n");
    double MSE2 = 0;
    Frame* out2 = getDifFrame(predict, frame2, &MSE2);

    //Output MSE
    printf("MSE: %f\n", MSE2);

    //Try to open output file
    printf("Open file ../videodecoder/output_m2_error.yuv\n");
    FILE* fp_out2 = fopen("../videodecoder/output_m2_error.yuv", "wb");
    if(fp_out2 == NULL)
    {
        printf("File write error!");
        return 1;
    }

    //Output dif frame
    printf("Output frame data\n");
    writeFrameY(fp_out2, out2);

    //Try to open output file
    printf("Open file ../videodecoder/output_m2_predict.yuv\n");
    FILE* fp_out3 = fopen("../videodecoder/output_m2_predict.yuv", "wb");
    if(fp_out3 == NULL)
    {
        printf("File write error!");
        return 1;
    }

    //Output dif frame
    printf("Output frame data\n");
    writeFrameY(fp_out3, predict);

    //Close file
    printf("Close file\n");
    fclose(fp_out3);



    //Free memory
    freeFrame(frame1);
    freeFrame(frame2);
    freeFrame(predict);
    freeFrame(out1);
    freeFrame(out2);


    printf("\nFinish!\n\n");
    return 0;
}
