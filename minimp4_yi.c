#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

#define VIDEO_FPS 20

static FILE *preload(const char *path, ssize_t *data_size)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    *data_size = 0;
    if (file == NULL)
        return NULL;
    if (fseek(file, 0, SEEK_END))
        return NULL;
    *data_size = (ssize_t)ftell(file);
    if (*data_size < 0)
        return NULL;
    if (fseek(file, 0, SEEK_SET))
        return NULL;
    return file;
}

typedef struct
{
    FILE *file;
    ssize_t size;
} INPUT_BUFFER;

static int read_callback(int64_t offset, void *buffer, size_t size, void *token)
{
    INPUT_BUFFER *buf = (INPUT_BUFFER*)token;
    size_t to_copy = MINIMP4_MIN(size, buf->size - offset - size);
    fseek(buf->file, offset, SEEK_SET);
    fread(buffer, 1, to_copy, buf->file);
    return to_copy != size;
}

int demux(FILE *input_file, ssize_t input_size, FILE *fout, int ntrack)
{
    int /*ntrack, */i, vpsspspps_bytes;
    const void *vpsspspps;
    INPUT_BUFFER buf = { input_file, input_size };
    MP4D_demux_t mp4 = { 0, };
    MP4D_open(&mp4, read_callback, &buf, input_size);
    int iframe_found = 0;

    //for (ntrack = 0; ntrack < mp4.track_count; ntrack++)
    {
        MP4D_track_t *tr = mp4.track + ntrack;
        unsigned sum_duration = 0;
        i = 0;
        if (tr->handler_type == MP4D_HANDLER_TYPE_VIDE)
        {   // assume h264
            char sync[4] = { 0, 0, 0, 1 };
            while (vpsspspps = MP4D_read_vps(&mp4, ntrack, i, &vpsspspps_bytes))
            {
                fwrite(sync, 1, 4, fout);
                fwrite(vpsspspps, 1, vpsspspps_bytes, fout);
                i++;
            }
            i = 0;
            while (vpsspspps = MP4D_read_sps(&mp4, ntrack, i, &vpsspspps_bytes))
            {
                fwrite(sync, 1, 4, fout);
                fwrite(vpsspspps, 1, vpsspspps_bytes, fout);
                i++;
            }
            i = 0;
            while (vpsspspps = MP4D_read_pps(&mp4, ntrack, i, &vpsspspps_bytes))
            {
                fwrite(sync, 1, 4, fout);
                fwrite(vpsspspps, 1, vpsspspps_bytes, fout);
                i++;
            }
            if (mp4.track[ntrack].sample_count == 0)
            {
                return -1;
            }
            for (i = 0; i < mp4.track[ntrack].sample_count; i++)
            {
                unsigned frame_bytes, timestamp, duration;
                MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frame_bytes, &timestamp, &duration);
                uint8_t *mem;
                fseek(input_file, ofs, SEEK_SET);
                sum_duration += duration;
                while (frame_bytes)
                {
                    mem = (uint8_t *) malloc(4);
                    fread(mem, 1, 4, input_file);
                    uint32_t size = ((uint32_t)mem[0] << 24) | ((uint32_t)mem[1] << 16) | ((uint32_t)mem[2] << 8) | mem[3];
                    free(mem);
                    if (size > 1048576)
                    {
                        printf("error: wrong frame size\n");
                        exit(-11);
                    }
                    mem = (uint8_t *) malloc(size + 4);
                    fread(&mem[4], 1, size, input_file);
                    size += 4;
                    mem[0] = 0; mem[1] = 0; mem[2] = 0; mem[3] = 1;
                    if (((mem[4] & 0x1F) == 0x05) || ((mem[4] & 0x7E) == 0x26))
                    {
                        fwrite(mem, 1, size, fout);
                        iframe_found = 1;
                        free(mem);
                        mem = NULL;
                        break;
                    }
                    if (frame_bytes < size)
                    {
                        printf("error: demux sample failed\n");
                        free(mem);
                        mem = NULL;
                        exit(-12);
                    }
                    frame_bytes -= size;
                }
                if (mem != NULL) free(mem);
                if (iframe_found == 1) break;
            }
        }
    }

    MP4D_close(&mp4);

    return 0;
}

int main(int argc, char **argv)
{
    // check switches
    int track = 0;
    int i;
    for(i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
            break;
        switch (argv[i][1])
        {
        case 't': i++; if (i < argc) track = atoi(argv[i]); break;
        default:
            printf("error: unrecognized option\n");
            return 1;
        }
    }
    if (argc <= (i + 1))
    {
        printf("Usage: minimp4 [options] input output\n"
               "Options:\n"
               "    -t    - de-mux tack number\n");
        return 0;
    }
    ssize_t h26x_size;
    // Remember to close buf_h26x
    FILE *buf_h26x = preload(argv[i], &h26x_size);
    if (buf_h26x == NULL)
    {
        printf("error: can't open h264 file\n");
        exit(-1);
    }

    FILE *fout = fopen(argv[i + 1], "wb");
    if (!fout)
    {
        printf("error: can't open output file\n");
        exit(-2);
    }

    int ret = demux(buf_h26x, h26x_size, fout, track);
    if (ret < 0) {
        remove(argv[i + 1]);
    }

    if (buf_h26x != NULL)
        fclose(buf_h26x);
    if (fout)
        fclose(fout);

    return ret;
}
