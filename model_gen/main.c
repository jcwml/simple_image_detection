// https://github.com/jcwml
// a cs:go head trigger bot

#include <dirent.h> 

#include <stdio.h>
#include <math.h>

#define pix unsigned char

const int dump_code = 0;

typedef struct
{
    pix cmin, cmax;
    unsigned int min, max, tmin, tmax, avg;
} pmap;

pix tol1 = 16, tol2 = 138;

pmap target[784] = {0}; // 28*28

void init_pmap(pmap* p)
{
    for(int i = 0; i < 784; i++)
    {
        p[i].avg = 0;

        p[i].cmax = 0;
        p[i].cmin = 255;

        p[i].max = 0;
        p[i].min = 0;
        p[i].tmax = 0;
        p[i].tmin = 0;
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
                        if(p < target[i].cmin)
                        {
                            target[i].cmin = p;
                            target[i].min += p;
                            target[i].tmin++;
                        }
                        if(p > target[i].cmax)
                        {
                            target[i].cmax = p;
                            target[i].max += p;
                            target[i].tmax++;
                        }
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
        target[i].max /= target[i].tmax;
        target[i].min /= target[i].tmin;
        printf("[%i] %u / %u %u\n", i, target[i].avg, target[i].min, target[i].max);
    }

    printf("\n******************************\n\n");

    // find how many pixels are over tol2
    unsigned int over = 0;
    pix lowest = 255;
    for(int i = 0; i < 784; i++)
    {
        const pix d = target[i].max - target[i].min;

        if(d < lowest)
            lowest = d;
        
        if(d > tol2)
        {
            over++;
            if(dump_code == 0)
                printf("[%i] %u\n", i, d);
        }
        else
        {
            const int y = i/28;
            const int x = i-(y*28);
            if(dump_code == 1)
                printf("ipit(%i, %i, %i.f);\n", x, y, target[i].avg);
        }
    }
    if(dump_code == 0)
    {
        printf("over: %i/784\n", over);
        printf("low:  %i\n", lowest);
    }

    // save pixel map
    save_pmap(&target[0]);

}