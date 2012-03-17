/*
 * rs_irgen_sharp_a486jb.c
 * generates IR signal data for KURO-RS
 * for SHARP A486JB
 *
 * Copyright (C) 2011,2012 n13i
 *
 * WARNING: This program expects running on little endian machines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define T_ON (488)
#define T_OFF (464)
#define T_TRAIL (8000)

// エンコード後のデータ長
#define MAX_ENCDATA_LENGTH (1920)  // 1920ビット
#define MAX_ENCBYTE_LENGTH (240)   // 240バイト

// エンコード前のデータ長
#define DATA_LENGTH (13)

#define UNSET (-1)
#define TRUE (1)
#define FALSE (0)

typedef unsigned char U8;

typedef union
{
    U8 c;
    struct {
        unsigned b0 : 1;
        unsigned b1 : 1;
        unsigned b2 : 1;
        unsigned b3 : 1;
        unsigned b4 : 1;
        unsigned b5 : 1;
        unsigned b6 : 1;
        unsigned b7 : 1;
    } b;
} CHARBITS;

typedef union
{
    U8 c;
    struct {
        unsigned lower : 4;
        unsigned upper : 4;
    } nibbles;
} NIBBLES;

// SHARP製エアコン用リモコンA486JBの解析結果に基づく
// https://docs.google.com/spreadsheet/pub?hl=ja&hl=ja&key=0Ar6zS-xs16bVdEoxUm9iRFB4VGVYdDNfWkZYMkZxVkE&output=html
typedef union
{
    U8 c[DATA_LENGTH];
    struct {
        U8 maker_code[2];
        unsigned maker_code_parity    : 4;
        unsigned system_code          : 4;
        unsigned product_code         : 8;
        unsigned temp                 : 4;
        unsigned dummy1_00010000      : 8;
        unsigned cmd                  : 4;
        unsigned mode                 : 2;
        unsigned dummy2_00            : 2;
        unsigned set_volume           : 1;
        unsigned volume               : 2;
        unsigned dummy3_0             : 1;
        unsigned timer_hour           : 4;
        unsigned dummy4_0             : 1;
        unsigned timer_1hoff          : 1;
        unsigned timer_mode           : 2;
        unsigned direction            : 3;
        unsigned dummy5_1000000000001 : 13;
        unsigned fullpower            : 1;
        unsigned timer_30min          : 4;
        unsigned eco                  : 1;
        unsigned dummy6_1111000000    : 10;
        unsigned dummy6_0001          : 4;
        unsigned checksum             : 4;
    } s;
} SIGNAL;

// 入力データをKURO-RS向け赤外線波形データに変換する
// 家製協(AEHA)フォーマット
// 参考: http://elm-chan.org/docs/ir_format.html
void encode_aeha(U8 data[], int length, U8 out[])
{
    int i;
    int pos = 0;
    int endt = 0;

    struct {
        unsigned int on;
        unsigned int off;
    } pulse_length[MAX_ENCDATA_LENGTH];
    int cnt = 0;

    // リーダー部
    pulse_length[0].on = 8*T_ON;
    pulse_length[0].off = 4*T_OFF;
    cnt++;

    // データ部
    for(i = 0; i < length; i++)
    {
        int j;
        for(j = 0; j < 8; j++)
        {
            U8 b = (data[i] >> j) & 0x1;
            pulse_length[cnt].on = T_ON;
            pulse_length[cnt].off = (b == 1 ? 3*T_OFF : T_OFF);
            cnt++;
        }
    }

    // トレイラー部
    pulse_length[cnt].on = T_ON;
    pulse_length[cnt].off = T_TRAIL;
    cnt++;

    // KURO-RS向けに100us単位のON/OFFデータに変換する
    for(i = 0; i < cnt; i++)
    {
        int j;
        int endpos;
        //printf("ON=%5d, OFF=%5d\n", pulse_length[i].on, pulse_length[i].off);

        // 0: ON1T/OFF1T
        // 1: ON1T/OFF3T
        endt += pulse_length[i].on;
        endpos = endt / 100;
        for(j = pos; j < endpos; j++)
        {
            if(j >= MAX_ENCDATA_LENGTH) { continue; }
            out[j] = 1;
            pos = j+1;
        }
        endt += pulse_length[i].off;
        endpos = endt / 100;
        for(j = pos; j < endpos; j++)
        {
            if(j >= MAX_ENCDATA_LENGTH) { continue; }
            out[j] = 0;
            pos = j+1;
        }
    }
}

void usage(int argc, char **argv)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s -p power -t temp -m mode -v volume\n"
        "  %s -f [0|1]\n\n"
        "     -p: power: 0:off 1:on\n"
        "     -t: temp:  18 to 32\n"
        "     -m: mode:  0:auto 1:heat 2:cool 3:dry\n"
        "     -v: volume: 0:auto 1 2 3\n"
        "     -f: fullpower: 0:off 1:on\n"
        "", argv[0], argv[0]
    );
}

int main(int argc, char **argv)
{
    int i;
    int cmdPower = UNSET, cmdTemp = UNSET, cmdMode = UNSET, cmdVolume = UNSET;
    int cmdIsFullPower = FALSE;
    U8 out[MAX_ENCDATA_LENGTH];
    U8 outbyte[MAX_ENCBYTE_LENGTH];
    SIGNAL s;
    int opt;

    for(i = 0; i < DATA_LENGTH; i++) { s.c[i] = 0; }
    for(i = 0; i < MAX_ENCDATA_LENGTH; i++) { out[i] = 0; }

    while((opt = getopt(argc, argv, "p:t:m:v:f:")) != -1)
    {
        switch(opt)
        {
        case 'p':
            cmdPower = atoi(optarg);
            break;
        case 't':
            cmdTemp = atoi(optarg);
            break;
        case 'm':
            cmdMode = atoi(optarg);
            break;
        case 'v':
            cmdVolume = atoi(optarg);
            break;
        case 'f':
            cmdIsFullPower = atoi(optarg);
            break;
        default:
            usage(argc, argv);
            exit(EXIT_FAILURE);
        }
    }

    fprintf(stderr,
        "power = %d, temp = %d, mode = %d, volume = %d\n",
        cmdPower, cmdTemp, cmdMode, cmdVolume);
    if((cmdPower != 0 && cmdPower != 1) ||
       (cmdTemp < 18 || cmdTemp > 32) ||
       (cmdMode < 0 || cmdMode > 3) ||
       (cmdVolume < 0 || cmdVolume > 3) ||
       (cmdIsFullPower != UNSET && cmdIsFullPower != 0 && cmdIsFullPower != 1)
    )
    {
        usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    // 信号データのセット

    // 未解析部分(固定データ？)
    s.s.dummy1_00010000 = 0x10;
    s.s.dummy2_00 = 0x0;
    s.s.dummy3_0 = 0x0;
    s.s.dummy4_0 = 0x0;
    s.s.dummy5_1000000000001 = 0x1001;
    s.s.dummy6_1111000000 = 0x3c0;
    s.s.dummy6_0001 = 0x1;

    // メーカーコード・システムコード(固定値)
    s.s.maker_code[0] = 0xaa;
    s.s.maker_code[1] = 0x5a;
    s.s.maker_code_parity = 0xf;
    s.s.system_code = 0xc;
    s.s.product_code = 0x10;

    // コマンドデータ
    s.s.temp = cmdTemp - 17;
    if(cmdIsFullPower == UNSET)
    {
        s.s.cmd = (cmdPower == 1 ? 1 : 2);
    }
    else
    {
        s.s.cmd = (cmdIsFullPower == 1 ? 6 : 7);
    }
    s.s.mode = cmdMode;
    s.s.set_volume = (cmdVolume == 0 ? 0 : 1);
    s.s.volume = (cmdVolume == 1 ? 1 : cmdVolume);
    s.s.timer_hour = 0;
    s.s.timer_1hoff = 0;
    s.s.timer_mode = 0;
    s.s.direction = 0;
    s.s.fullpower = (cmdIsFullPower != UNSET ? 1 : 0);
    s.s.timer_30min = 0;
    s.s.eco = 0;
    s.s.checksum = 0;
    
    // チェックサムの計算
    // 3バイト目の下位～13バイト目の上位まで4ビットずつXOR
    for(i = 2; i < DATA_LENGTH; i++)
    {
        NIBBLES n;
        n.c = s.c[i];
        if(i > 2)
        {
            s.s.checksum ^= n.nibbles.lower;
        }
        if(i < 12)
        {
            s.s.checksum ^= n.nibbles.upper;
        }
    }

#ifdef DEBUG
    for(i = 0; i < DATA_LENGTH; i++)
    {
        printf("%02x ", s.c[i]);
    }
    printf("\n");
#endif

#ifdef DEBUG
    for(i = 0; i < DATA_LENGTH; i++)
    {
        CHARBITS b;
        b.c = s.c[i];
        printf("%d%d%d%d%d%d%d%d",
            b.b.b0,
            b.b.b1,
            b.b.b2,
            b.b.b3,
            b.b.b4,
            b.b.b5,
            b.b.b6,
            b.b.b7);
    }
    printf("\n");
#endif

    for(i = 0; i < MAX_ENCDATA_LENGTH; i++) { out[i] = 0; }

    encode_aeha(s.c, DATA_LENGTH, out);

#ifdef DEBUG
    for(i = 0; i < MAX_ENCDATA_LENGTH; i++)
    {
        printf("%d", out[i]);
    }
    printf("\n");
#endif

    // KURO-RS向けの16進文字列データに変換する
    for(i = 0; i < MAX_ENCBYTE_LENGTH; i++)
    {
        CHARBITS b;
        b.b.b0 = out[i*8+0];
        b.b.b1 = out[i*8+1];
        b.b.b2 = out[i*8+2];
        b.b.b3 = out[i*8+3];
        b.b.b4 = out[i*8+4];
        b.b.b5 = out[i*8+5];
        b.b.b6 = out[i*8+6];
        b.b.b7 = out[i*8+7];
        outbyte[i] = b.c;
        printf("%02x", outbyte[i]);
    }

    return 0;
}

