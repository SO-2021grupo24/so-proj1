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

    for (int i = 0; i < 12; ++i) {
        r = tfs_write(f, str, strlen(str));
        assert(r == strlen(str));
    }

    assert(tfs_close(f) != -1);

    f = tfs_open(path, TFS_O_TRUNC);
    assert(f != -1);

    printf("Successful test.\n");

    return 0;
}
