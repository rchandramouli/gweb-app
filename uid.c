/*
 * Unique UID allocation
 *
 * 64-bit UID is computed as bit-permute within nibble and nibble
 * permutation of 10-digit phone number (total of 40-bits) with a
 * right rotate of 18-bits along with 24-bit buzHash of e-mail and
 * phone number.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#define MAX_DIGITS   (10)

/* BUZ hash bucket */
static uint32_t BuzBucket[256] = {
    0xd5d934d7, 0x8706252b, 0x2cce7ec5, 0x8cccba6f, 0x829d80de, 0xf619b4c8, 0x37dea8a3, 0x85b62cda, 
    0xe123f4f8, 0x6ff8e0ed, 0x25e0373e, 0xde72f815, 0xf086136c, 0xefcb091b, 0xa5aad393, 0x89022c36, 
    0x9fefb351, 0x5488a5f6, 0x439652d2, 0xeeafccc0, 0xec7628fc, 0xb5152e96, 0xeb6e3c8f, 0xbcd71fe9, 
    0x11a12340, 0x4682edb2, 0x6db4953e, 0xd376b52b, 0xb83484cb, 0xe19dfe60, 0x5160f515, 0x5c51d620, 
    0x5d1f29eb, 0x6aca89b7, 0x3505e843, 0xd0c5eb68, 0x635b08f3, 0xf0ae1dce, 0x0d694a13, 0xf8f877b4, 
    0x9bf5011c, 0xa0f71db3, 0x4158a17c, 0x6d5b123e, 0xa3ac3ddc, 0xa0d64f92, 0x04663845, 0x6968a4c2, 
    0x9abe01e9, 0x3abd4ebb, 0xc7b275b4, 0x26de4400, 0x543c7470, 0x6d7fd60a, 0xd71750fa, 0x55bcd453, 
    0x7e2b5f4e, 0x2be9c9b5, 0xc652496a, 0xb3c32244, 0x5b535e5d, 0x7ddc5757, 0x577e88a3, 0xdd4aa993, 
    0x428afad2, 0xd8d7fae6, 0xb1abac0e, 0x748a5a4b, 0xfd47a616, 0xb6fafacf, 0x4ec090a2, 0x88e42536, 
    0xc5a73106, 0x6938d7cc, 0xe49ea82b, 0x613845aa, 0x3b2ca95c, 0x6698605e, 0x14a96594, 0x3fa1c3cf, 
    0xcd13dca1, 0x9198db3c, 0xd24c786c, 0x6c40c85e, 0xd510daa4, 0x7a646d2e, 0xa6e8de60, 0x7708dbd8, 
    0xde644f3e, 0x27953ad3, 0x306cd8fb, 0xd29431d3, 0xfbd002ac, 0xd5a39a0e, 0x0c413894, 0x428275bb, 
    0x759b8d8f, 0xaa8dfa1b, 0x9d276697, 0xd3f3b256, 0x77c402af, 0x93fa3c71, 0x605d535a, 0x92ffb9e4, 
    0x728ec16c, 0x531ded6e, 0xf9f89fa8, 0x3df02727, 0xbd04e77d, 0xca9ea384, 0x36eedcb1, 0xf62363aa, 
    0xe0a5168f, 0x364f8879, 0xadf96e81, 0xb540e159, 0x3afb2033, 0x37f00ee9, 0x593c57e1, 0x5bd7f6ce, 
    0x90649bfc, 0x9bba3fa2, 0xcc69b9eb, 0x6a627607, 0x059a686b, 0xb82ec4e7, 0xc4d2cc57, 0x77362784, 
    0x838117d5, 0x27864784, 0xbd083d6e, 0xbd699c8b, 0x7eee9d47, 0xbebfa642, 0xf4fad3fd, 0xe75fc622, 
    0x5edf8cd6, 0x740024df, 0xc89795c0, 0xac07e245, 0x03e27341, 0xd2a1a8bc, 0xb24af048, 0xdb2a2231, 
    0x7c53851f, 0xc40dc034, 0x19e07092, 0x5d4a0af1, 0x73906996, 0x941ef6af, 0xe80083a1, 0x7def581b, 
    0xeac81121, 0x6698907b, 0xb9d8c4da, 0x1c0163ef, 0x3c728d58, 0x8763257b, 0x9028f43c, 0xfa485d6a, 
    0x0411e0ca, 0x23a35a69, 0xa0444c38, 0xd6e92a5b, 0x2a14f91d, 0xf0a18c08, 0x2c8c5b9d, 0x126793d0, 
    0x67facdf3, 0x912b2815, 0xd342aac0, 0x2caca6fa, 0xd88c2c02, 0x077b7db0, 0x08d1b2db, 0xb86021ec, 
    0x59d7fcc6, 0xc131146e, 0xf92ac333, 0x024394e0, 0x8f0c4575, 0x3b8e3507, 0x69ba2192, 0x5a4f3a84, 
    0x0ad257de, 0x6ddaab34, 0xd93c4bc3, 0x8dd46e47, 0x5c4efbf9, 0xcda4c75a, 0x35f8119b, 0x3be7ba5f, 
    0xdf048e21, 0xc5caf105, 0x856c9862, 0x61606215, 0x57c7580b, 0xfe999a9d, 0x1632d0d5, 0x86f1d193, 
    0x2c31ba8b, 0x09130295, 0x0cafec05, 0x70a082f6, 0xcd9bd3e1, 0xb47c2f11, 0x8fe3d6b6, 0x994e128c, 
    0xd09a54bf, 0xb7cbf68f, 0xaf0e37d6, 0x02a0f4e5, 0xd59c3fc4, 0x78f74324, 0x33a2f8df, 0x78134efd, 
    0x5a56a3a2, 0x79b8cff1, 0x2b3f025b, 0x2034b0a5, 0x40175710, 0x828eedae, 0xd36260d8, 0xb499810c, 
    0x5c5eafd9, 0x838ea18d, 0x790a76d8, 0x9f938caa, 0xd010ea73, 0x63ad2615, 0xe161232d, 0x7c72cc87, 
    0x09a54f80, 0x7781067d, 0x64bb7da0, 0xd6e8f917, 0xd4149706, 0x14527194, 0x32a22a88, 0xc9d95016, 
    0x1394a149, 0x6a954537, 0xdec2e35f, 0xe9c69f6e, 0x916c42d1, 0x6c73bcfb, 0x17ae026e, 0x3310a1b1, 
    0x8c616e9f, 0x5bd947b2, 0xffdf90be, 0xf3fb136c, 0xcda63f75, 0xe542423a, 0xd8dc98cb, 0x9b52e55a,
};

static uint64_t rrotN (uint64_t nr, int fl, int n)
{
    uint64_t nrot, mask;

    mask = (1ULL << (uint64_t)fl) - 1;
    nrot = nr & mask;
    nrot = (nrot >> (uint64_t)n) |
        ((nrot & ((1ULL << (uint64_t)n) - 1)) << (uint64_t)(fl - n));

    return ((nr & ~mask) | (nrot & mask));
}

static uint64_t
do_buz_hash (const char *msg)
{
    uint64_t h = 0xF6CCCA28ULL;

    while (*msg)
        h = rrotN(h, 32, 1) ^ BuzBucket[(uint32_t)*msg++];

    return h;
}

/* Interleave two 32-bit quantities into 64-bit by picking up
 * nibbles alternately
 */
static uint64_t
interleave_nibble (uint64_t n1, uint64_t n2)
{
    uint64_t n = 0, i;

    for (i = 0; i < 8; i++) {
        n |= (((n1 & 0xF) << 4) | (n2 & 0xF)) << (i * 8);
        n1 >>= 4;
        n2 >>= 4;
    }

    return n;
}

static void
deinterleave_nibble (uint64_t n, uint64_t *n1, uint64_t *n2)
{
    uint64_t i;

    *n1 = *n2 = 0;
    for (i = 0; i < 8; i++) {
        *n1 |= ((n & 0xF0) >> 4) << (i * 4);
        *n2 |= (n & 0xF) << (i * 4);
        n >>= 8;
    }
}

/* Convert 40-bit phone to string */
static void
int_to_phone (uint64_t phone, char *pstr)
{
    int i;

    for (i = 0; i < MAX_DIGITS; i++) {
        pstr[i] = '0' + ((phone >> ((uint64_t)(MAX_DIGITS-i-1) << 2)) & 0xF);
    }
    pstr[i] = '\0';
}

static uint64_t
phone_to_int (const char *phone)
{
    uint64_t nr = 0, shift = 60;
    int v;

    while (shift && *phone) {
        v = (*phone) - '0';
        if (v < 0 || v > 9) {
            phone++;
            continue;
        }
        nr |= ((uint64_t)v & 0xFULL) << shift;
        phone++;
        shift -= 4;
    }

    return nr;
}

/* Does permutation/inverse of: 7530289641 bytes, with 0 being the LSN */
static int p_encode[] = { 7, 5, 3, 0, 2, 8, 9, 6, 4, 1 };
static int p_decode[] = { 3, 9, 4, 2, 8, 1, 7, 0, 5, 6 };

static uint64_t
permute_phone_digits (uint64_t nr_phone, int *pindex)
{
    uint64_t permuted_nr = 0, v;
    int i;

    for (i = 0; i < MAX_DIGITS; i++) {
        v = (nr_phone & (0xFULL << ((uint64_t)i << 2)));
        if (i < pindex[i]) {
            permuted_nr |= (v << ((pindex[i] - i) << 2));
        } else if (i > pindex[i]) {
            permuted_nr |= (v >> ((i - pindex[i]) << 2));
        } else {
            permuted_nr |= v;
        }
    }
    return permuted_nr;
}

/* 1234 <-> 3142 */
static uint64_t
permute_nibble_bits3142 (uint64_t nr)
{
    uint64_t permuted_nr = 0, v, i;

    for (i = 0; i < 16; i++) {
        v = (nr >> (i << 2)) & 0xF;
        v = (((v << 2) | (v >> 2)) & 9) | ((v >> 1) & 4) | ((v << 1) & 2);
        permuted_nr |= (v & 0xF) << (i << 2);
    }
    return permuted_nr;
}

/* Convert given 40-bit integer to base-10 integer */
static uint64_t
convert_to_b10int (uint64_t nr)
{
    uint64_t b10_nr = 0, i;

    for (i = 0; i < MAX_DIGITS; i++) {
        b10_nr = b10_nr*10 + (nr & 0xF);
        nr >>= 4;
    }
    return b10_nr;
}

/* Convert given base-10 integer to 40-bit integer */
static uint64_t
convert_to_b4int (uint64_t nr)
{
    uint64_t nr_40bit = 0, i;

    for (i = 0; i < MAX_DIGITS; i++) {
        nr_40bit |= (nr % 10) << ((MAX_DIGITS-i-1) << 2);
        nr /= 10;
    }
    return nr_40bit;
}

uint64_t
gweb_app_get_uid (const char *phone, const char *email)
{
    uint64_t nr_phone;
    uint64_t hash_code;
    uint64_t uid;

    if (phone == NULL || email == NULL)
        return 0;
    
    /* Pick top 40-bits of phone, permute and convert it to 32-bits
     * (note: we require a maximum of 34 bits, so, we steal 2 bits
     * from hash code.
     */
    nr_phone = (phone_to_int(phone) >> 24ULL) & 0x000000FFFFFFFFFFULL;
    nr_phone = permute_phone_digits(nr_phone, p_encode);
    nr_phone = convert_to_b10int(nr_phone);

    /* 32-bit BUZ hash */
    hash_code = do_buz_hash(phone);
    hash_code ^= do_buz_hash(email);

    /* Encode 2-bits from phone to hash code */
    hash_code &= ~0x3ULL;
    hash_code |= (nr_phone & 0x3ULL);
    nr_phone = (nr_phone >> 2) & 0xFFFFFFFFULL;
    
    /* Interleave phone and hash code */
    uid = interleave_nibble(nr_phone, hash_code);
    uid = permute_nibble_bits3142(uid);

    return uid;
}

static const char *Base64String =
    "XalEVJWsU6DyFueK_890zhdPvxTHc.3gGMim7kQInqrfpoBNw1jYZS5t4LACRbO2";

void
gweb_app_get_uid_str (const char *phone, const char *email, char *uid_str)
{
    uint64_t uid;

    if (uid_str == NULL)
        return;

    uid = gweb_app_get_uid(phone, email);
    while (uid) {
        *uid_str++ = Base64String[uid & 0x3F];
        uid >>= 6;
    }
    *uid_str = '\0';
}

static uint64_t
uid_str_to_uid (const char *uid_str)
{
    uint64_t uid = 0, i = 0, idx;

    while (*uid_str) {
        for (idx = 0; Base64String[idx]; idx++)
            if (Base64String[idx] == *uid_str)
                break;
        if (idx < 64) {
            uid |= (idx << (i * 6));
            i++;
        }
        uid_str++;
    }

    return uid;
}

#ifdef TEST
struct test_vectors {
    char *phone;
    char *email;
};

int main (int argc, char *argv[])
{
    struct test_vectors tv[] = {
        { "0123456789", "test@example.com" },
        { "0123456789", "retest@example.com" },
        { "0101010101", "test@example.com" },
        { "8178364862", "sample@yourspace.org" },
        { "0000010002", "test@example.com" },
        { "1111111111", "sample@yourspace.org" },
        { "0000000000", "test@example.com" },
        { "0000000001", "test@example.com" },
    };
    uint64_t uid, uidx, u, v, w;
    char phone[MAX_DIGITS+1], uid_str[16];
    int idx;

    for (idx = 0; idx < sizeof(tv)/sizeof(struct test_vectors); idx++) {
        uid = gweb_app_get_uid((const char *)tv[idx].phone,
                               (const char *)tv[idx].email);
        gweb_app_get_uid_str((const char *)tv[idx].phone,
                             (const char *)tv[idx].email,
                             uid_str);

        uidx = uid_str_to_uid(uid_str);
        v = permute_nibble_bits3142(uidx);
        v = permute_nibble_bits3142(v);
        v = permute_nibble_bits3142(v);
        deinterleave_nibble(v, &u, &w);

        u <<= 2;
        u |= (w & 0x3);
        v = convert_to_b4int(u);
        v = permute_phone_digits(v, p_decode);
        int_to_phone(v, phone);

        printf("P2I = %"PRIx64", I2P = %s\n", phone_to_int(tv[idx].phone), phone);
        printf("Test vector: %d (%s, %s)\nUID = 0x%"PRIx64" (%s), "
               "UID-X = 0x%"PRIx64"\n",
               idx+1, tv[idx].phone, tv[idx].email, uid, uid_str, uidx);
        printf("\n");
    }
    return 0;
}
#endif // TEST
