#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

 /*
 * des takes two arguments on the command line:
 *    des -enc -ecb      -- encrypt in ECB mode
 *    des -enc -ctr      -- encrypt in CTR mode
 *    des -dec -ecb      -- decrypt in ECB mode
 *    des -dec -ctr      -- decrypt in CTR mode
 * des also reads some hardcoded files:
 *    message.txt            -- the ASCII text message to be encrypted,
 *                              read by "des -enc"
 *    encrypted_msg.bin      -- the encrypted message, a binary file,
 *                              written by "des -enc"
 *    decrypted_message.txt  -- the decrypted ASCII text message
 *    key.txt                -- just contains the key, on a line by itself, as an ASCII 
 *                              hex number, such as: 0x34FA879B
*/

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////
typedef uint64_t KEYTYPE;
typedef uint32_t SUBKEYTYPE;
typedef uint64_t BLOCKTYPE;

struct BLOCK {
    BLOCKTYPE block;        // the block read
    int size;               // number of "real" bytes in the block, should be 8, unless it's the last block
    struct BLOCK *next;     // pointer to the next block
};
typedef struct BLOCK* BLOCKLIST;

/* global variable*/
uint64_t initialKey;
uint64_t iv = 0x135792468000000;//just a random number for CTR mode
#define LOW32 0xffffffff
#define LOW48 0xffffffffffff
/*global variable end*/

/* function declaration*/
static void freeBlocks(BLOCKLIST blocks);
static uint64_t permute(uint64_t n, uint32_t permBox[], uint32_t len);
/////////////////////////////////////////////////////////////////////////////
// Initial and final permutation
/////////////////////////////////////////////////////////////////////////////
uint32_t init_perm[] = {
	58,50,42,34,26,18,10,2,
	60,52,44,36,28,20,12,4,
	62,54,46,38,30,22,14,6,
	64,56,48,40,32,24,16,8,
	57,49,41,33,25,17,9,1,
	59,51,43,35,27,19,11,3,
	61,53,45,37,29,21,13,5,
	63,55,47,39,31,23,15,7
};

uint32_t final_perm[] = {
	40,8,48,16,56,24,64,32,
	39,7,47,15,55,23,63,31,
	38,6,46,14,54,22,62,30,
	37,5,45,13,53,21,61,29,
	36,4,44,12,52,20,60,28,
	35,3,43,11,51,19,59,27,
	34,2,42,10,50,18,58,26,
	33,1,41,9, 49,17,57,25
};
/*helper and debugging functions*/
void printBinary(uint64_t n) {
    int i;
    int count = 0;
    for (i = 0; i < 64; i++){
        uint64_t current = n & 0x8000000000000000;
        current = current >> 63;
        printf("%lu", current);
        count++;
        if (count == 8) {
            printf(" ");
            count = 0;
        }
        n = n << 1;
    }
    printf("\n");
}

/////////////////////////////////////////////////////////////////////////////
// Subkey generation
/////////////////////////////////////////////////////////////////////////////

// This function returns the i:th subkey, 48 bits long. To simplify the assignment 
// you can use a trivial implementation: just take the input key and xor it with i,
uint64_t getSubKey(int i, KEYTYPE key) {
   // return the first 48 bits of the 56 bit DES key, xor:ed with i.
    return (key ^ i) & LOW48;
}

// For extra credit, write the real DES key expansion routine!
void generateSubKeys(KEYTYPE key) {
   // TODO for extra credit
}

/////////////////////////////////////////////////////////////////////////////
// P-boxes
/////////////////////////////////////////////////////////////////////////////
uint32_t expand_box[] = {
	32,1,2,3,4,5,4,5,6,7,8,9,
	8,9,10,11,12,13,12,13,14,15,16,17,
	16,17,18,19,20,21,20,21,22,23,24,25,
	24,25,26,27,28,29,28,29,30,31,32,1
};

uint32_t Pbox[] = 
{
	16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
	2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25,
};		

/////////////////////////////////////////////////////////////////////////////
// S-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t sbox_1[4][16] = {
	{14,  4, 13,  1,  2, 15, 11,  8,  3, 10 , 6, 12,  5,  9,  0,  7},
	{ 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
	{ 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
	{15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}};

uint64_t sbox_2[4][16] = {
	{15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5 ,10},
	{ 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
	{ 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
	{13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}};

uint64_t sbox_3[4][16] = {
	{10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
	{13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
	{13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
	{ 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}};


uint64_t sbox_4[4][16] = {
	{ 7, 13, 14,  3,  0 , 6,  9, 10,  1 , 2 , 8,  5, 11, 12,  4 ,15},
	{13,  8, 11,  5,  6, 15,  0,  3,  4 , 7 , 2, 12,  1, 10, 14,  9},
	{10,  6,  9 , 0, 12, 11,  7, 13 ,15 , 1 , 3, 14 , 5 , 2,  8,  4},
	{ 3, 15,  0,  6, 10,  1, 13,  8,  9 , 4 , 5, 11 ,12 , 7,  2, 14}};
 
 
uint64_t sbox_5[4][16] = {
	{ 2, 12,  4,  1 , 7 ,10, 11,  6 , 8 , 5 , 3, 15, 13,  0, 14,  9},
	{14, 11 , 2 ,12 , 4,  7, 13 , 1 , 5 , 0, 15, 10,  3,  9,  8,  6},
	{ 4,  2 , 1, 11, 10, 13,  7 , 8 ,15 , 9, 12,  5,  6 , 3,  0, 14},
	{11,  8 ,12 , 7 , 1, 14 , 2 ,13 , 6 ,15,  0,  9, 10 , 4,  5,  3}};


uint64_t sbox_6[4][16] = {
	{12,  1, 10, 15 , 9 , 2 , 6 , 8 , 0, 13 , 3 , 4 ,14 , 7  ,5 ,11},
	{10, 15,  4,  2,  7, 12 , 9 , 5 , 6,  1 ,13 ,14 , 0 ,11 , 3 , 8},
	{ 9, 14 ,15,  5,  2,  8 ,12 , 3 , 7 , 0,  4 ,10  ,1 ,13 ,11 , 6},
	{ 4,  3,  2, 12 , 9,  5 ,15 ,10, 11 ,14,  1 , 7  ,6 , 0 , 8 ,13}};
 

uint64_t sbox_7[4][16] = {
	{ 4, 11,  2, 14, 15,  0 , 8, 13, 3,  12 , 9 , 7,  6 ,10 , 6 , 1},
	{13,  0, 11,  7,  4 , 9,  1, 10, 14 , 3 , 5, 12,  2, 15 , 8 , 6},
	{ 1 , 4, 11, 13, 12,  3,  7, 14, 10, 15 , 6,  8,  0,  5 , 9 , 2},
	{ 6, 11, 13 , 8,  1 , 4, 10,  7,  9 , 5 , 0, 15, 14,  2 , 3 ,12}};
 
uint64_t sbox_8[4][16] = {
	{13,  2,  8,  4,  6 ,15 ,11,  1, 10,  9 , 3, 14,  5,  0, 12,  7},
	{ 1, 15, 13,  8 ,10 , 3  ,7 , 4, 12 , 5,  6 ,11,  0 ,14 , 9 , 2},
	{ 7, 11,  4,  1,  9, 12, 14 , 2,  0  ,6, 10 ,13 ,15 , 3  ,5  ,8},
	{ 2,  1, 14 , 7 , 4, 10,  8, 13, 15, 12,  9,  0 , 3,  5 , 6 ,11}};

/*permutation functions*/
// combine all permutation functions into one
static uint64_t permute(uint64_t n, uint32_t permBox[], uint32_t len){
    int i;
    uint64_t result = 0;
    for (i = 0; i < len; i++){
        uint32_t originalPosition = permBox[i];
        //extract bit at originalPosition
        uint64_t temp = (n >> (originalPosition - 1)) & 0x1;
        //printf("%lu\n", temp);
        if (temp != 0) {
            //put it back in result
            temp = temp << i;
            //printf("%d %lu\n", i, temp);
            result = result | temp;
        }
    }
    return result;
}

/*
uint64_t initPerm (uint64_t n){
    int i;
    uint64_t result = 0;
    for (i = 0; i < 64; i++){
        uint32_t originalPosition = init_perm[i];
        //extract bit at originalPosition
        uint64_t temp = (n >> (originalPosition - 1)) & 0x1;
        //printf("%lu\n", temp);
        if (temp != 0) {
            //put it back in result
            temp = temp << i;
            //printf("%d %lu\n", i, temp);
            result = result | temp;
        }
    }
    return result;
}

uint64_t finalPerm (uint64_t n){
    int i;
    uint64_t result = 0;
    for (i = 0; i < 64; i++){
        uint32_t originalPosition = final_perm[i];
        //extract bit at originalPosition
        uint64_t temp = (n >> (originalPosition - 1)) & 0x1;
        //printf("%lu\n", temp);
        if (temp != 0) {
            //put it back in result
            temp = temp << i;
            //printf("%d %lu\n", i, temp);
            result = result | temp;
        }
    }
    return result;
}
//expand right 32 bits to 48 bits
uint64_t expanPerm(uint64_t n){
    int i;
    uint64_t result = 0;
    for (i = 0; i < 48; i++){
        uint32_t originalPosition = expand_box[i];
        //extract bit at originalPosition
        uint64_t temp = (n >> (originalPosition - 1)) & 0x1;
        //printf("%lu\n", temp);
        if (temp != 0) {
            //put it back in result
            temp = temp << i;
            //printf("%d %lu\n", i, temp);
            result = result | temp;
        }
    }
    return result;
}


uint64_t pBoxPerm(uint64_t n) {
    int i;
    uint64_t result = 0;
    for (i = 0; i < 32; i++){
        uint32_t originalPosition = Pbox[i];
        //extract bit at originalPosition
        uint64_t temp = (n >> (originalPosition - 1)) & 0x1;
        //printf("%lu\n", temp);
        if (temp != 0) {
            //put it back in result
            temp = temp << i;
            //printf("%d %lu\n", i, temp);
            result = result | temp;
        }
    }
    return result;
}
*/
uint64_t f(uint64_t right, uint64_t roundKey){
    // expand v from 32 bits to 48 bits
    uint64_t result = 0;
    //uint64_t expandR = expanPerm(right);
    uint64_t expandR = permute(right, expand_box, 48);//new
    // XOR expandR and roundKey
    expandR = expandR ^ roundKey;
    int i;
    uint64_t subst;
    // send to 8 s_boxes
    for (i = 0; i < 8; i++){
        uint64_t current = expandR >> i*6; // move corresponding 6 bits to lowest position
        current = current & 0x3f; //extract first 6 bits
        uint64_t col = (current >> 1 ) & 0xf; //extract b2 to b5
        uint64_t bitOne = current & 0x1; // extract b1
        uint64_t bitSix = (current >> 5) & 0x1; // extract b6
        uint64_t row = (bitSix << 1) | bitOne; // connect b1 b6
        //printf("row: %lu col: %lu\n", row, col);
        if ( i == 0){
            subst = sbox_1[row][col];
        }
        else if (i == 1){
            subst = sbox_2[row][col];
        }
        else if (i == 2){
            subst = sbox_3[row][col];
        }
        else if (i == 3){
            subst = sbox_4[row][col];
        }
        else if (i == 4){
            subst = sbox_5[row][col];
        }
        else if (i == 5){
            subst = sbox_6[row][col];
        }
        else if (i == 6){
            subst = sbox_7[row][col];
        }
        else {
            subst = sbox_8[row][col];
        }
        //printf("%lu\n", subst);
        subst = subst << i*4;
        result = result | subst;
    }
    //return pBoxPerm(result);
    return permute(result, Pbox, 32);//new
}

/////////////////////////////////////////////////////////////////////////////
// I/O
/////////////////////////////////////////////////////////////////////////////

// Pad the list of blocks, so that every block is 64 bits, even if the
// file isn't a perfect multiple of 8 bytes long. In the input list of blocks,
// the last block may have "size" < 8. In this case, it needs to be padded. See 
// the slides for how to do this (the last byte of the last block 
// should contain the number if real bytes in the block, add an extra block if
// the file is an exact multiple of 8 bytes long.) The returned
// list of blocks will always have the "size"-field=8.
// Example:
//    1) The last block is 5 bytes long: [10,20,30,40,50]. We pad it with 2 bytes,
//       and set the length to 5: [10,20,30,40,50,0,0,5]. This means that the 
//       first 5 bytes of the block are "real", the last 3 should be discarded.
//    2) The last block is 8 bytes long: [10,20,30,40,50,60,70,80]. We keep this 
//       block as is, and add a new final block: [0,0,0,0,0,0,0,0]. When we decrypt,
//       the entire last block will be discarded since the last byte is 0
void pad_last_block(BLOCKLIST blocks) {
    // TODO
    BLOCKLIST lastBlock = blocks;
    while(lastBlock->next != NULL) {
        lastBlock = lastBlock->next;
    }
    int size = lastBlock->size;
    if (size == 0){
        lastBlock->block = 0;
    }
    else {
        BLOCKTYPE temp = 0;
        //here assume little endian
        char * ptr = (char*)&temp;
        ptr[7] = (char)size;
        //printf("%lu\n", temp);
        memcpy((char *)&temp, (char *)&(lastBlock->block), size);
        lastBlock->block = temp;
    }
}

// Reads the message to be encrypted, an ASCII text file, and returns a linked list 
// of BLOCKs, each representing a 64 bit block. In other words, read the first 8 characters
// from the input file, and convert them (just a C cast) to 64 bits; this is your first block.
// Continue to the end of the file.
BLOCKLIST read_cleartext_message(FILE *msg_fp) {
    // TODO
    BLOCKLIST head = NULL;
    BLOCKLIST current = NULL;
    char buffer[8];
    size_t size = 0;
    while(1) {
        size = fread(buffer, 1, 8, msg_fp);
        if (head == NULL){
            head = malloc(sizeof(struct BLOCK));
            head->block = *((BLOCKTYPE *)buffer);
            head->size = (int)size;
            head->next = NULL;
            current = head;
        }
        else {
            current->next = malloc(sizeof(struct BLOCK));
            current = current->next;
            current->block = *((BLOCKTYPE *)buffer);
            current->size = (int)size;
            current->next = NULL;
        }
        if (size != 8) break;
    }
    // call pad_last_block() here to pad the last block!
    pad_last_block(head);
    return head;
}

// Reads the encrypted message, and returns a linked list of blocks, each 64 bits. 
// Note that, because of the padding that was done by the encryption, the length of 
// this file should always be a multiople of 8 bytes. The output is a linked list of
// 64-bit blocks.
BLOCKLIST read_encrypted_file(FILE *msg_fp) {
    BLOCKLIST head = NULL;
    BLOCKLIST current = NULL;
    char buffer[8];
    size_t size = 0;
    while(1) {
        size = fread(buffer, 1, 8, msg_fp);
        //printf("size is %lu\n", size);
        if (size != 8) break;//end of file
        if (head == NULL){
            head = malloc(sizeof(struct BLOCK));
            head->block = *((BLOCKTYPE *)buffer);
            head->size = 8;
            head->next = NULL;
            current = head;
        }
        else {
            current->next = malloc(sizeof(struct BLOCK));
            current = current->next;
            current->block = *((BLOCKTYPE *)buffer);
            current->size = 8;
            current->next = NULL;
        }
    }
    return head;
}

// Reads 56-bit key into a 64 bit unsigned int. We will ignore the most significant byte,
// i.e. we'll assume that the top 8 bits are all 0. In real DES, these are used to check 
// that the key hasn't been corrupted in transit. The key file is ASCII, consisting of
// exactly one line. That line has a single hex number on it, the key, such as 0x08AB674D9.
KEYTYPE read_key(FILE *key_fp) {
    // TODO
    char * line = NULL;
    size_t len = 0;
    uint64_t key = 0;
    if (getline(&line, &len, key_fp) != -1) {
        //printf("%s", line);//line already contains \n
        char * ptr;
        key = strtoul(line,&ptr,16);
        //set top 8 bits to 0
        key = key & 0xffffffffffffff;
    }
    else {
        printf("getline return -1\n");
    }
    return key;
}

// Write the encrypted blocks to file. The encrypted file is in binary, i.e., you can
// just write each 64-bit block directly to the file, without any conversion.
void write_encrypted_message(FILE *msg_fp, BLOCKLIST msg) {
    if (msg == NULL){
        printf("write_encrypted_message: nothing to write, msg is NULL\n");
        return;
    }
    BLOCKLIST current = msg;
    while (current != NULL){
        void * ptr = (void *)&(current->block);
        fwrite(ptr, 8, 1, msg_fp);
        current = current->next;
    }
}

// Write the encrypted blocks to file. This is called by the decryption routine.
// The output file is a plain ASCII file, containing the decrypted text message.
void write_decrypted_message(FILE *msg_fp, BLOCKLIST msg) {
    if (msg == NULL){
        printf("write_decrypted_message: nothing to write, msg is NULL\n");
        return;
    }
    BLOCKLIST current = msg;
    while (current->next != NULL){
        void * ptr = (void *)&(current->block);
        fwrite(ptr, 8, 1, msg_fp);
        current = current->next;
    }
    // for the last block
    if (*(uint64_t *)&(current->block) == 0){
        current->size = 0;
        return;
    }
    char * cptr = (char *)&(current->block);
    current->size = (int)cptr[7];
    fwrite(cptr, 1, current->size, msg_fp);
}

/////////////////////////////////////////////////////////////////////////////
// Encryption
/////////////////////////////////////////////////////////////////////////////

// Encrypt one block. This is where the main computation takes place. It takes
// one 64-bit block as input, and returns the encrypted 64-bit block. The 
// subkeys needed by the Feistel Network is given by the function getSubKey(i).
BLOCKTYPE des_enc(BLOCKTYPE v, KEYTYPE key){
    //v = initPerm(v);
    v = permute(v, init_perm, 64);//new
    //extract left and right
    uint64_t right = v & LOW32;
    uint64_t left = (v >> 32);
    //printf("%lu\n",left);
    //printf("%lu\n",right);
    int i;
    for (i = 1; i <= 16; i++){
        uint64_t  roundKey = getSubKey(i,key);
        //printf("%lu\n",key);
        uint64_t temp = f(right,roundKey);
        temp = temp ^ left;
        left = right;
        right = temp;
    }
    // reverse left and right
    uint64_t result = (right << 32) | left;
    //return finalPerm(result);
    return permute(result, final_perm, 64);//new
}

// Encrypt the blocks in ECB mode. The blocks have already been padded 
// by the input routine. The output is an encrypted list of blocks.
BLOCKLIST des_enc_ECB(BLOCKLIST msg) {
    // Should call des_enc in here repeatedly
    BLOCKLIST current = msg;
    while(current != NULL){
        current->block = des_enc(current->block, initialKey);
        current = current->next;
    }
    return msg;
}

// Same as des_enc_ECB, but encrypt the blocks in Counter mode.
// SEE: https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Counter_(CTR)
// Start the counter at 0.
BLOCKLIST des_enc_CTR(BLOCKLIST msg) {
    uint64_t counter = 0;
    BLOCKLIST current = msg;
    
    /* previous CTR mode (possible wrong)
    while(current != NULL){
        current->block = des_enc(current->block, initialKey + counter);
        current = current->next;
        counter++;
    }
    */
    //correct CTR mode?
    while(current != NULL) {
        uint64_t cipher = des_enc(iv + counter, initialKey);
        current->block = current->block ^ cipher;
        current = current->next;
        counter++;
    }
    return msg;
}

/////////////////////////////////////////////////////////////////////////////
// Decryption
/////////////////////////////////////////////////////////////////////////////
// Decrypt one block.
BLOCKTYPE des_dec(BLOCKTYPE v, KEYTYPE key){
    //v = initPerm(v);
    v = permute(v, init_perm, 64);//new
    //extract left and right
    uint64_t right = v & LOW32;
    uint64_t left = (v >> 32);
    //printf("%lu\n",left);
    //printf("%lu\n",right);
    int i;
    for (i = 16; i >= 1; i--){
        uint64_t  roundKey = getSubKey(i, key);
        //printf("%lu\n",key);
        uint64_t temp = f(right,roundKey);
        temp = temp ^ left;
        left = right;
        right = temp;
    }
    // reverse left and right
    uint64_t result = (right << 32) | left;
    //return finalPerm(result);
    return permute(result, final_perm, 64);//new
}

// Decrypt the blocks in ECB mode. The input is a list of encrypted blocks,
// the output a list of plaintext blocks.
BLOCKLIST des_dec_ECB(BLOCKLIST msg) {
    BLOCKLIST current = msg;
    while(current != NULL){
        current->block = des_dec(current->block, initialKey);
        current = current->next;
    }
    return msg;    
}

// Decrypt the blocks in Counter mode
BLOCKLIST des_dec_CTR(BLOCKLIST msg) {
    return des_enc_CTR(msg);//same as encryption
    //uint64_t counter = 0;
    //BLOCKLIST current = msg;
    /* previous CTR mode (possible wrong)
    while(current != NULL){
        current->block = des_dec(current->block, initialKey + counter);
        current = current->next;
        counter++;
    }
    */
    /*
    while(current != NULL) {
        uint64_t cipher = des_enc(iv + counter, initialKey);
        current->block = current->block ^ cipher;
        current = current->next;
        counter++;
    }
    return msg;
    */
}

/////////////////////////////////////////////////////////////////////////////
// Main routine
/////////////////////////////////////////////////////////////////////////////

void encrypt (int argc, char **argv) {
     //printf("encrypt start\n");
     FILE *msg_fp = fopen("message.txt", "r");
     BLOCKLIST msg = read_cleartext_message(msg_fp);
     fclose(msg_fp);

     BLOCKLIST encrypted_message;
     if (strcmp(argv[2], "-ecb") == 0) {	
         encrypted_message = des_enc_ECB(msg);
     } else if (strcmp(argv[2], "-ctr") == 0) {	
         encrypted_message = des_enc_CTR(msg);
     } else {
         printf("No such mode.\n");
     };
     FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "wb");
     write_encrypted_message(encrypted_msg_fp, encrypted_message);
     fclose(encrypted_msg_fp);
     if (encrypted_message != NULL) {
         freeBlocks(encrypted_message);
     }
     //printf("encrypt end\n");
}

void decrypt (int argc, char **argv) {
      //printf("decrypt start\n");
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "r");
      BLOCKLIST encrypted_message = read_encrypted_file(encrypted_msg_fp);
      fclose(encrypted_msg_fp);

      BLOCKLIST decrypted_message;
      if (strcmp(argv[2], "-ecb") == 0) {	
         decrypted_message = des_dec_ECB(encrypted_message);
      } else if (strcmp(argv[2], "-ctr") == 0) {	
         decrypted_message = des_dec_CTR(encrypted_message);
      } else {
         printf("No such mode.\n");
      };

      FILE *decrypted_msg_fp = fopen("decrypted_message.txt", "w");
      write_decrypted_message(decrypted_msg_fp, decrypted_message);
      fclose(decrypted_msg_fp);
      
      if (decrypted_message != NULL) {
          freeBlocks(decrypted_message);
      }
      
      //printf("decrypt end\n");
}
void freeBlocks(BLOCKLIST head) {
    BLOCKLIST current;
    while(head != NULL){
        current = head;
        head = head->next;
        free(current);
    }
}
void printBlocks(BLOCKLIST head){
    while (head != NULL){
        printf("%d\n", head->size);
        printBinary(head->block);
        head = head->next;
    }
}
int main(int argc, char **argv){
    if (argc < 3) {
        printf("not engough arguments\n");
        return 0;
    }
    FILE *key_fp = fopen("key.txt","r");
    initialKey = read_key(key_fp);
    //generateSubKeys(key);              // This does nothing right now.
    fclose(key_fp);
    //printf("%lu\n", key);
    //printBinary(key);
    
    //read message
    //FILE *msg_fp = fopen("message.txt", "r");
    //BLOCKLIST msg = read_cleartext_message(msg_fp);
    //fclose(msg_fp); 
    //printBlocks(msg);
    //freeBlocks(msg);
   //printf("%s\n", argv[1]);
   //printf("%s\n", argv[2]);
   if (strcmp(argv[1], "-enc") == 0) {
      encrypt(argc, argv);	
   } else if (strcmp(argv[1], "-dec") == 0) {
      decrypt(argc, argv);	
   } else {
     printf("First argument should be -enc or -dec\n"); 
   }

   return 0;
}
