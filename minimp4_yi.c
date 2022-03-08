#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

#define VIDEO_FPS 20

static uint8_t *preload(const char *path, ssize_t *data_size)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    *data_size = 0;
    if (!file)
        return 0;
    if (fseek(file, 0, SEEK_END))
        exit(1);
    *data_size = (ssize_t)ftell(file);
    if (*data_size < 0)
        exit(1);
    if (fseek(file, 0, SEEK_SET))
        exit(1);
    data = (unsigned char*)malloc(*data_size);
    if (!data)
        exit(1);
    if ((ssize_t)fread(data, 1, *data_size, file) != *data_size)
        exit(1);
    fclose(file);
    return data;
}

typedef struct
{
    uint8_t *buffer;
    ssize_t size;
} INPUT_BUFFER;

static int read_callback(int64_t offset, void *buffer, size_t size, void *token)
{
    INPUT_BUFFER *buf = (INPUT_BUFFER*)token;
    size_t to_copy = MINIMP4_MIN(size, buf->size - offset - size);
    memcpy(buffer, buf->buffer + offset, to_copy);
    return to_copy != size;
}

int demux(uint8_t *input_buf, ssize_t input_size, FILE *fout, int ntrack)
{
    int /*ntrack, */i, vpsspspps_bytes;
    const void *vpsspspps;
    INPUT_BUFFER buf = { input_buf, input_size };
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
#define USE_SHORT_SYNC 0
            char sync[4] = { 0, 0, 0, 1 };
            while (vpsspspps = MP4D_read_vps(&mp4, ntrack, i, &vpsspspps_bytes))
            {
                fwrite(sync + USE_SHORT_SYNC, 1, 4 - USE_SHORT_SYNC, fout);
                fwrite(vpsspspps, 1, vpsspspps_bytes, fout);
                i++;
            }
            i = 0;
            while (vpsspspps = MP4D_read_sps(&mp4, ntrack, i, &vpsspspps_bytes))
            {
                fwrite(sync + USE_SHORT_SYNC, 1, 4 - USE_SHORT_SYNC, fout);
                fwrite(vpsspspps, 1, vpsspspps_bytes, fout);
                i++;
            }
            i = 0;
            while (vpsspspps = MP4D_read_pps(&mp4, ntrack, i, &vpsspspps_bytes))
            {
                fwrite(sync + USE_SHORT_SYNC, 1, 4 - USE_SHORT_SYNC, fout);
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
                uint8_t *mem = input_buf + ofs;
                sum_duration += duration;
                while (frame_bytes)
                {
                    uint32_t size = ((uint32_t)mem[0] << 24) | ((uint32_t)mem[1] << 16) | ((uint32_t)mem[2] << 8) | mem[3];
                    size += 4;
                    mem[0] = 0; mem[1] = 0; mem[2] = 0; mem[3] = 1;
                    if ((mem[4] == 0x65) || (mem[4] == 0x26)) {
                        fwrite(mem + USE_SHORT_SYNC, 1, size - USE_SHORT_SYNC, fout);
                        iframe_found = 1;
                        break;
                    }
                    if (frame_bytes < size)
                    {
                        printf("error: demux sample failed\n");
                        exit(1);
                    }
                    frame_bytes -= size;
                    mem += size;
                }
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
    // Remember to free buf_h26x
    uint8_t *buf_h26x = preload(argv[i], &h26x_size);
    if (!buf_h26x)
    {
        printf("error: can't open h264 file\n");
        exit(1);
    }

    FILE *fout = fopen(argv[i + 1], "wb");
    if (!fout)
    {
        printf("error: can't open output file\n");
        exit(1);
    }

    int ret = demux(buf_h26x, h26x_size, fout, track);
    if (ret < 0) {
        remove(argv[i + 1]);
    }

    if (buf_h26x)
        free(buf_h26x);
    if (fout)
        fclose(fout);

    return ret;
}
