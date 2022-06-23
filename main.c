// https://github.com/jcwml
// a cs:go head trigger bot

/*

    the plan was to generate a greyscale pixel map from the RGB sample images
    and then discard the pixels with a max-min deviance that was too large
    so that less pixels needed to be scanned and only the areas of low deviance
    could be focused on.
    
    then, I would test that pixel map of low deviance pixels against the original
    image samples and their nontarget samples increasing the tolerance on each epoch
    (the tolerance being a range value from the avg pixel colour that is accepted)
    until all 255 tolerance values are tested and calculate which tolerance value
    had the best detection on targets and least error on nontargets.

    this would leave me with a concise pixelmap with good detection results.

    ... only as you can see the lowest deviance per grayscale pixel in the target
    dataset is 229 which means this won't work on this particular dataset.

    but it's a cool idea that might work on something else, like a runescape bot.

    the final pixel map could generate C source code calling a is_pixel_in_tol(x, y, tol)
    function for each individual pixel that passed the minimum deviance test.

    very efficient, no loops, very compact, ready to deploy.

*/

#include <dirent.h> 

#include <stdio.h>
#include <math.h>

#define pix unsigned char

typedef struct
{
    pix min, max;
    unsigned int avg;
} pmap;

pix tol1 = 16, tol2 = 96;

pmap target[784] = {0}; // 28*28

void init_pmap(pmap* p)
{
    for(int i = 0; i < 784; i++)
    {
        p[i].avg = 0;
        p[i].max = 0;
        p[i].min = 255;
    }
}

void save_pmap(pmap* p)
{
    FILE* f = fopen("pixmap.dat", "wb");
    if(f != NULL)
    {
        if(fwrite(p, sizeof(pmap), 784, f) < 784)
            printf("ERR: pixmap save.");
        fclose(f);
    }
}

void load_pmap(pmap* p)
{
    FILE* f = fopen("pixmap.dat", "rb");
    if(f != NULL)
    {
        if(fread(p, sizeof(pmap), 784, f) < 784)
            printf("ERR: pixmap load.");
        fclose(f);
    }
}

int main()
{
    // init pixel map
    init_pmap(&target[0]);

    // load dataset and produce pixmap
    unsigned int total_samples = 0;
    struct dirent *dir;
    DIR* d = opendir("target/");
    if(d != NULL)
    {
        while((dir = readdir(d)) != NULL)
        {
            if(dir->d_name[0] != '.')
            {
                printf("%s\n", dir->d_name);
                total_samples++;

                char fp[384];
                sprintf(fp, "target/%s", dir->d_name);
                pix pb[2352]; // 28*28*3
                FILE* f = fopen(fp, "rb");
                if(f != NULL)
                {
                    fseek(f, 13, SEEK_SET);
                    if(fread(&pb, 1, sizeof(pb), f) < sizeof(pb))
                        printf("ERR: %s load.", dir->d_name);
                    fclose(f);

                    for(int i = 0; i < 784; i++)
                    {
                        const int j = i*3;
                        const pix p = (pb[j] + pb[j+1] + pb[j+2]) / 3; // average to greyscale value
                        target[i].avg += p;
                        if(p < target[i].min)
                            target[i].min = p;
                        if(p > target[i].max)
                            target[i].max = p;
                    }
                }
            }
        }
        closedir(d);
    }

    // finish the averages
    for(int i = 0; i < 784; i++)
    {
        target[i].avg /= total_samples;
        printf("[%i] %u\n", i, target[i].avg);
    }

    printf("\n******************************\n\n");

    // find how many pixels are over tol2
    unsigned int over = 0;
    pix lowest = 255;
    for(int i = 0; i < 784; i++)
    {
        const pix d = target[i].max - target[i].min;
        if(d > tol2)
        {
            over++;
            if(d < lowest)
                lowest = d;
            printf("[%i] %u\n", i, d);
        }
    }
    printf("over: %i/784\n", over);
    printf("low:  %i\n", lowest);

    // save pixel map
    save_pmap(&target[0]);

}
