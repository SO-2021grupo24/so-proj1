#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ARR_SZ 7000

int main() {
    /* Should segfault if 1st exercise not yet done (size is too big). */
    char str[ARR_SZ] =
        "splkclgmrlheaopfslzpdglgoopadnonvezwpgnqwsidtgmjtyqebkshfwyabxjnikgcbc"
        "mnrnyrrobugchebrlgzbvnqgftsoypaokreofmauscgrhwngukuzbglptbmbjbtmwkiwpu"
        "ueazayowjjdcurvgwhnobjqclzicqmquffmezyvtfiymvhjiodmvvnydvwflphjzwnjwfk"
        "kjtlxnmnzivgoczrbprqlugnfdmncfvfpvqnnctazmqhpjnvsxvhrjcyqlxlznttgjsflq"
        "wzalgkuulhcgmwzmjlufiwmcwbnnzxhczpriughqyvwpggguysfawdafjrdpxkfwcirqdy"
        "qaazzkxothrhgserdplbpsjnfvsjibpzsrldsfcwmqzjgcyfowugybprhxsritrjszafqf"
        "lbklmfptpsgvypkwffwxteeedffnwqdjuscskfrhhayrbhojdyhmbfdjexrhnmmnxgomde"
        "fcyxmpnsfucbjmgtlvgrsvvbdvluwpsvfrnitckzshkurfxdelhqwejkyzgzvcplrxtcde"
        "wsxjrfuzslxkjdzaabnppbcckslfvhoexbiofhjpnhnyorfzcdiuivtrxvoacrompiuysx"
        "eqlxynwzdxfwyetgintldnxwruvrvgfvtruqsvjudrlokcciflwxfsmndrsdyzjesngrcn"
        "lkanjotcucuyqmwylmtjjzofcacwsiywyxiysptvgvxdrjedlbogysvhfbigtismgqfswl"
        "kciiabjeeddaxygbuxagbmmordillyflowjtwhnsthnhzitabcgygbgsncrmjebaujhsdm"
        "uisvuuofguntnfefuodzifkwgvvdzxqubveqowvqwpennylrwmwszvbbkpjinqsudxqmdj"
        "ftsbzrwvhbbpitooaiarmyckvxvwutzrfxgcgivrysrqwwkcmbhlmoiqftiysfgpbkelqr"
        "ifmhgpnikqnpjhbmcpsranruuqhbkhglmeexkukwowmemblinxcegnjfsosuabrltleuot"
        "yswtnriijvysenzljjidxnacmiqqyayvxopesmnrjvbldmlldfkhmqarhidbtkxbdlgmrl"
        "epsovvlpwpgyvcwwphvuvphqfzhrzrjffcpcbnrfooubdiopxxbxcrzadcxgrtgouhgxhe"
        "pnreaeeqtmpvxaqfcmsojyhhjtstkytlynnwasdasoafmkpnocehccqzvkcjhwshjfwsus"
        "srgspifqxemelozgbrcjisqziermrkndnapskvijpczemfzbdvblhzpszsvhwoxbvzfqon"
        "yplsvoxrqkaroxddtbqkleeanehelzflqammfakpneljeklrkizecokyrusbmedxvxyvfv"
        "ueicybtpnnnemukefxclkqpeiazqiglmmwfpatkzzchqmunfjfowsbvcmlzdvxcdvdmoan"
        "nsznpprirjbstkzyjojwmydfphrmrsjtzgljhfuywwfrgkplwccrhkkrzsuzuoccyocchd"
        "fezjgxgstvcezcorueevejknttaiegbhrbvkdinugifwodwsyxpfywryjwytvbsfpxjqij"
        "kqzwldrsbfwxisvkkerwzpzixojadituebcxrfirlgidijkgiuojdgorpogekuzrrrystn"
        "mzfefafmtwzewnmgfotachszwmenajfnnerfsvsoxsvkjrrrhcpnumxyqopfluiydnthfb"
        "fdidfnkfxobfncwunuaigpymdergcyobysyrysmdicvuyiywzhwvroavndcnjlvwyfxzvz"
        "qdprwpuimlktvugnipcqbhgnesiikvshvadwwslhtoqiwtpqsjnzdvzbgjtsxehklssyud"
        "fjlazxrdredvgsrqcvbpbzflrcfahadbnntokcgwxuxzkqkwzgkywolrxfnnfslwttmlsi"
        "apboobhroazigvyskkitllqmfskaepjdxjakfssxliobngsonatuzgcvviggannqavylsv"
        "vvildghhzpuagmchmu"; // 2048 chars
    char buffer[ARR_SZ];
    char *path = "/f1";
    /*
            for(int i = 0; i < 2048; ++i)
                    str[i] = 'a';
    */
    str[2048] = '\0';

    assert(tfs_init() != -1);

    int f;
    ssize_t r;

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str));

    buffer[r] = '\0';
    assert(strcmp(buffer, str) == 0);

    int f2, f3;

    f2 = tfs_open("/lol", TFS_O_CREAT);
    f3 = tfs_open("/lel", TFS_O_CREAT);

    assert(f2 != -1);
    assert(f3 != -1);

    const char *const s2 = "oi";

    r = tfs_write(f2, s2, strlen(s2));
    assert(r == strlen(s2));
    const char *const s3 = "miga";
    r = tfs_write(f3, s2, strlen(s2));
    assert(r == strlen(s2));

    r = tfs_write(f2, s3, strlen(s3));
    assert(r == strlen(s3));

    assert(tfs_close(f2) != -1);
    assert(tfs_close(f3) != -1);

    f2 = tfs_open("/lol", 0);

    assert(f2 != -1);

    r = tfs_read(f2, buffer, strlen(s2) + strlen(s3));
    assert(r == strlen(s2) + strlen(s3));
    buffer[strlen(s2) + strlen(s3)] = '\0';

    assert(!strcmp(buffer, "oimiga"));

    assert(tfs_close(f) != -1);

    printf("Successful test.\n");

    return 0;
}

#if 0
const char str[] =
"abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcX";

int main() {
    char buf[100];
   
    assert(tfs_init() != -1);
    
    (void)buf;
    int f;
    ssize_t r;


    f = tfs_open("/f1", TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str, sizeof(str) - 1);
    printf("%ld\n", r);
    //assert(r == sizeof(str) - 1);

    __asm__ __volatile__("nop");

    assert(tfs_close(f) != -1);

    printf("Successful test.\n");

    return 0;
}
#endif
