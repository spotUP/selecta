/* Per-song gain and peak limiter (loudness_gain.h). */
#include "loudness_gain.h"

/* 10^(+-d/20) for d = 0..24 dB and 10^(+-c/2000) for c = 0..99 centi-dB, in fixed point (positive dB table Q28,
 * the rest Q30). The product of one entry from each is exact to well under 1 LSB of 16.16 over the +-24 dB
 * clamp. Generated once; nothing is computed at run time (no float on the 68k). */
static const uint32_t k_pos_db_q28[25] = {
    268435456u, 301189535u, 337940217u, 379175160u, 425441527u, 477353244u,
    535599149u, 600952130u, 674279380u, 756553907u, 848867446u, 952444939u,
    1068660799u, 1199057137u, 1345364236u, 1509523501u, 1693713225u, 1900377495u,
    2132258619u, 2392433520u, 2684354560u, 3011895354u, 3379402170u, 3791751599u,
    4254415268u
};
static const uint32_t k_pos_cdb_q30[100] = {
    1073741824u, 1074978727u, 1076217055u, 1077456809u, 1078697991u, 1079940603u,
    1081184647u, 1082430123u, 1083677035u, 1084925383u, 1086175168u, 1087426394u,
    1088679061u, 1089933171u, 1091188725u, 1092445726u, 1093704175u, 1094964074u,
    1096225423u, 1097488226u, 1098752484u, 1100018198u, 1101285370u, 1102554002u,
    1103824095u, 1105095651u, 1106368672u, 1107643160u, 1108919116u, 1110196541u,
    1111475438u, 1112755808u, 1114037654u, 1115320975u, 1116605776u, 1117892056u,
    1119179818u, 1120469063u, 1121759794u, 1123052011u, 1124345717u, 1125640913u,
    1126937602u, 1128235784u, 1129535461u, 1130836636u, 1132139309u, 1133443483u,
    1134749160u, 1136056341u, 1137365027u, 1138675221u, 1139986924u, 1141300138u,
    1142614865u, 1143931107u, 1145248865u, 1146568140u, 1147888936u, 1149211253u,
    1150535093u, 1151860459u, 1153187351u, 1154515771u, 1155845722u, 1157177205u,
    1158510222u, 1159844774u, 1161180863u, 1162518492u, 1163857662u, 1165198374u,
    1166540631u, 1167884434u, 1169229785u, 1170576685u, 1171925138u, 1173275143u,
    1174626704u, 1175979822u, 1177334498u, 1178690735u, 1180048535u, 1181407898u,
    1182768827u, 1184131325u, 1185495391u, 1186861029u, 1188228240u, 1189597026u,
    1190967389u, 1192339331u, 1193712853u, 1195087957u, 1196464645u, 1197842919u,
    1199222781u, 1200604232u, 1201987275u, 1203371911u
};
static const uint32_t k_neg_db_q30[25] = {
    1073741824u, 956973408u, 852903448u, 760150998u, 677485290u, 603809400u,
    538145694u, 479622855u, 427464319u, 380977976u, 339546978u, 302621563u,
    269711752u, 240380852u, 214239660u, 190941298u, 170176611u, 151670064u,
    135176087u, 120475814u, 107374182u, 95697341u, 85290345u, 76015100u,
    67748529u
};
static const uint32_t k_neg_cdb_q30[100] = {
    1073741824u, 1072506344u, 1071272286u, 1070039648u, 1068808428u, 1067578625u,
    1066350237u, 1065123263u, 1063897700u, 1062673547u, 1061450803u, 1060229466u,
    1059009534u, 1057791006u, 1056573880u, 1055358154u, 1054143827u, 1052930897u,
    1051719364u, 1050509224u, 1049300476u, 1048093119u, 1046887152u, 1045682572u,
    1044479378u, 1043277569u, 1042077142u, 1040878097u, 1039680432u, 1038484144u,
    1037289233u, 1036095697u, 1034903534u, 1033712743u, 1032523322u, 1031335269u,
    1030148584u, 1028963264u, 1027779308u, 1026596714u, 1025415481u, 1024235607u,
    1023057091u, 1021879931u, 1020704125u, 1019529672u, 1018356571u, 1017184819u,
    1016014416u, 1014845359u, 1013677647u, 1012511279u, 1011346253u, 1010182568u,
    1009020222u, 1007859213u, 1006699539u, 1005541201u, 1004384195u, 1003228520u,
    1002074175u, 1000921159u, 999769469u, 998619104u, 997470063u, 996322344u,
    995175945u, 994030866u, 992887104u, 991744658u, 990603527u, 989463709u,
    988325202u, 987188005u, 986052117u, 984917536u, 983784260u, 982652289u,
    981521619u, 980392251u, 979264182u, 978137411u, 977011937u, 975887758u,
    974764872u, 973643278u, 972522975u, 971403961u, 970286234u, 969169794u,
    968054638u, 966940765u, 965828174u, 964716863u, 963606831u, 962498076u,
    961390597u, 960284392u, 959179459u, 958075799u
};

uint32_t sel_gain_q16_from_cdb(int32_t cdb) {
    uint32_t a;
    if (cdb > SEL_GAIN_MAX_CDB) cdb = SEL_GAIN_MAX_CDB;
    if (cdb < -SEL_GAIN_MAX_CDB) cdb = -SEL_GAIN_MAX_CDB;
    if (cdb >= 0) {
        a = (uint32_t)cdb;
        /* Q28 * Q30 = Q58, to Q16 */
        return (uint32_t)((((uint64_t)k_pos_db_q28[a / 100u] * k_pos_cdb_q30[a % 100u]) + ((uint64_t)1 << 41)) >> 42);
    }
    a = (uint32_t)-cdb;
    /* Q30 * Q30 = Q60, to Q16 */
    return (uint32_t)((((uint64_t)k_neg_db_q30[a / 100u] * k_neg_cdb_q30[a % 100u]) + ((uint64_t)1 << 43)) >> 44);
}

void sel_loudness_gain_set(sel_loudness_gain* g, uint32_t active, uint32_t gain_q16) {
    g->active = active != 0u;
    g->gain_q16 = gain_q16;
    g->limit_q16 = SEL_GAIN_UNITY;
}

/* Release: the reduction recovers by 1/1024 of the way back per frame (about 23 ms at 44.1 kHz), at least 1 LSB. */
void sel_loudness_gain_process(sel_loudness_gain* g, int16_t* stereo, uint32_t frames) {
    uint32_t i;
    if (!g->active || frames == 0u) {
        return;
    }
    g->frames += frames;
    for (i = 0; i < frames; i++) {
        int64_t l = ((int64_t)stereo[i * 2u] * g->gain_q16 + 32768) >> 16;
        int64_t r = ((int64_t)stereo[i * 2u + 1u] * g->gain_q16 + 32768) >> 16;
        int64_t m = (l < 0 ? -l : l) > (r < 0 ? -r : r) ? (l < 0 ? -l : l) : (r < 0 ? -r : r);
        uint32_t lim = g->limit_q16;
        if (lim < SEL_GAIN_UNITY) {
            lim += ((SEL_GAIN_UNITY - lim) >> 10) + 1u;
            if (lim > SEL_GAIN_UNITY) lim = SEL_GAIN_UNITY;
        }
        if (m * lim > ((int64_t)SEL_LIMIT_CEILING << 16)) {
            /* instant attack: the smallest reduction that puts this frame's peak at the ceiling */
            lim = (uint32_t)((((int64_t)SEL_LIMIT_CEILING << 16)) / m);
            g->hits++;
        }
        g->limit_q16 = lim;
        l = (l * lim) >> 16;
        r = (r * lim) >> 16;
        stereo[i * 2u] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
        stereo[i * 2u + 1u] = (int16_t)(r > 32767 ? 32767 : (r < -32768 ? -32768 : r));
    }
}

/* ---- adaptive leveler (I190) ---- */

int32_t sel_db10_cdb(uint64_t x) {
    uint32_t n = 0;
    uint64_t m;
    uint32_t frac = 0;
    uint32_t i;
    uint64_t log2_q16;
    if (x < 2u) {
        return 0;
    }
    while ((x >> (n + 1u)) != 0u) {
        n++;
    }
    /* mantissa in [1,2) as Q30, then the fraction of log2 by repeated squaring */
    m = n >= 30u ? x >> (n - 30u) : x << (30u - n);
    for (i = 0; i < 16u; i++) {
        m = (m * m) >> 30;
        frac <<= 1;
        if (m >= (2ull << 30)) {
            m >>= 1;
            frac |= 1u;
        }
    }
    log2_q16 = ((uint64_t)n << 16) | frac;
    return (int32_t)((log2_q16 * 30103u + (100ull << 15)) / (100ull << 16)); /* 3.0103 cdb per octave of power */
}

void sel_leveler_init(sel_leveler* lv) {
    lv->p = 0;
    lv->sum_e = 0;
    lv->sum_n = 0;
    lv->cur_q16 = 0;
    lv->want_cdb = 0;
    lv->frames = 0;
}

static int32_t lev_clamp(int32_t v) {
    return v > SEL_GAIN_MAX_CDB ? SEL_GAIN_MAX_CDB : (v < -SEL_GAIN_MAX_CDB ? -SEL_GAIN_MAX_CDB : v);
}

/* Gain in centi-dB that brings a per-frame mean of L^2+R^2 to the target, same unweighted RMS convention as the
 * analyser's fallback (sum over both channels, full scale 32768). */
static int32_t lev_gain_for(uint64_t mean) {
    int32_t loud = sel_db10_cdb(mean) - sel_db10_cdb(1ull << 30);
    return lev_clamp(SEL_LEVELER_TARGET_CDB - loud);
}

static int32_t lev_round(int64_t q16) {
    return (int32_t)((q16 + (q16 < 0 ? -32768 : 32768)) / 65536);
}

uint32_t sel_leveler_chunk(sel_leveler* lv, const int16_t* stereo, uint32_t frames, uint32_t rate) {
    uint64_t e = 0;
    uint32_t i;
    uint64_t mean;
    int64_t step;
    lv->frames += frames;
    for (i = 0; i < frames * 2u; i++) {
        e += (uint64_t)((int64_t)stereo[i] * stereo[i]);
    }
    mean = frames != 0u ? e / frames : 0u;
    if (mean >= 2000u) { /* gate: about -60 dBFS per channel; silence and quiet gaps do not move the estimate */
        uint64_t tau = (uint64_t)rate * SEL_LEVELER_TAU_MS / 1000u;
        uint64_t span;
        lv->sum_e += e;
        lv->sum_n += frames;
        span = lv->sum_n < tau ? lv->sum_n : tau;
        if (lv->p == 0u || span <= frames) {
            lv->p = mean;
        } else {
            int64_t d = (int64_t)mean - (int64_t)lv->p;
            lv->p = (uint64_t)((int64_t)lv->p + d * (int64_t)frames / (int64_t)span);
        }
        lv->want_cdb = lev_gain_for(lv->p);
    }
    step = (int64_t)SEL_LEVELER_SLEW_CDB_S * 65536 * frames / (rate != 0u ? rate : 1u);
    {
        int64_t want = (int64_t)lv->want_cdb * 65536;
        if (want > lv->cur_q16) {
            lv->cur_q16 = lv->cur_q16 + step > want ? want : lv->cur_q16 + step;
        } else {
            lv->cur_q16 = lv->cur_q16 - step < want ? want : lv->cur_q16 - step;
        }
    }
    return sel_gain_q16_from_cdb(lev_round(lv->cur_q16));
}

uint32_t sel_leveler_ceiling_q16(const sel_leveler* lv, uint32_t frames, uint32_t rate) {
    int64_t up = lv->cur_q16 + (int64_t)SEL_LEVELER_SLEW_CDB_S * 65536 * frames / (rate != 0u ? rate : 1u);
    return sel_gain_q16_from_cdb(lev_round(up) + 1);
}

int sel_leveler_estimate_cdb(const sel_leveler* lv, uint32_t rate, int32_t* cdb) {
    if (lv->sum_n < (uint64_t)rate * SEL_LEVELER_MIN_MS / 1000u || lv->sum_n == 0u) {
        return 0;
    }
    *cdb = lev_gain_for(lv->sum_e / lv->sum_n);
    return 1;
}
