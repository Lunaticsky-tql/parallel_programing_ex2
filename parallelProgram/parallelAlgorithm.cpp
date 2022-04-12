#include <iostream>
#include<sys/time.h>
#include<arm_neon.h>
#include<bitset>
#include<vector>

using namespace std;

FILE *fi;
FILE *fq;
struct POSTING_LIST {
    unsigned int len;
    unsigned int *arr;
} *posting_list_container;
double time_serial_bitmap;
double time_SIMD_bitmap;
double time_unrooled_bitmap;
double time_blocked_bitmap;
double time_SIMD_blocked_bitmap;
timeval tv_begin, tv_end;


const int POSTING_LIST_NUM = 1756;
unsigned int array_len;
unsigned int *temp_arr;
//25205174+1
const unsigned int MAX_DOC_ID = 25205175;
const int NEON_BLOCK_BIT = 128;
const unsigned int EXPANDED_BITMAP_SIZE = MAX_DOC_ID + 16 * NEON_BLOCK_BIT;
vector<vector<int> > query_list_container;
int posting_list_counter, query_list_count;
const unsigned int BLOCK_SIZE = 2048;


vector<int> to_int_list(char *line) {
    vector<int> int_list;
    int i = 0;
    int num = 0;
    while (line[i] == ' ' || (line[i] >= 48 && line[i] <= 57)) {
        num = 0;
        while (line[i] != ' ') {
            num *= 10;
            int tmp = line[i] - 48;
            num += tmp;
            i++;
        }
        i++;
        int_list.push_back(num);
    }
    return int_list;
}

int read_posting_list() {
    fi = fopen("/Users/tianjiaye/CLionProjects/SIMD/ExpIndex", "rb");
    if (NULL == fi) {
        printf("Can not open file ExpIndex!\n");
        return 1;
    }
    posting_list_container = (struct POSTING_LIST *) malloc(POSTING_LIST_NUM * sizeof(struct POSTING_LIST));
    if (NULL == posting_list_container) {
        printf("Can not malloc enough space\n");
        return 2;
    }
    while (1) {
        fread(&array_len, sizeof(unsigned int), 1, fi);
        if (feof(fi)) break;
        temp_arr = (unsigned int *) malloc(array_len * sizeof(unsigned int));
        for (int i = 0; i < array_len; i++) {
            fread(&temp_arr[i], sizeof(unsigned int), 1, fi);
            if (feof(fi)) break;
        }
        if (feof(fi)) break;
        POSTING_LIST tmp;
        tmp.len = array_len;
        tmp.arr = temp_arr;
        posting_list_container[posting_list_counter].len = array_len;
        posting_list_container[posting_list_counter].arr = temp_arr;
        posting_list_counter++;

    }
    fclose(fi);
    printf("posting_list num: %d\n", posting_list_counter);
    return 0;
}

int read_query_list() {
    fq = fopen("/Users/tianjiaye/CLionProjects/SIMD/ExpQuery", "r");
    if (NULL == fq) {
        printf("Can not open file ExpQuery!\n");
        return 1;
    }
    vector<int> query_list;
    char *line = new char[100];
    while ((fgets(line, 100, fq)) != NULL) {
        query_list = to_int_list(line);
        query_list_container.push_back(query_list);
        query_list_count++;
    }
    fclose(fq);
    printf("query_list num: %d\n", query_list_count);
    return 0;
}

unsigned int get_max_possible_common_DocId(POSTING_LIST *queried_posting_list, int query_word_num) {
    unsigned int max_possible_common_DocId = queried_posting_list[0].arr[queried_posting_list[0].len - 1];
    for (int i = 1; i < query_word_num; i++) {
        if (max_possible_common_DocId > queried_posting_list[i].arr[queried_posting_list[i].len - 1]) {
            max_possible_common_DocId = queried_posting_list[i].arr[queried_posting_list[i].len - 1];
        }
    }
    return max_possible_common_DocId;
}

void serial_bitmap_algorithm(POSTING_LIST *queried_posting_list, int query_word_num,
                             vector<unsigned int> &serial_bitmap_result) {
    //use bitset to store the posting list
    //get the min max_DocID of the posting list as max possible docID the result intersection can be
    unsigned int max_possible_common_DocId = get_max_possible_common_DocId(queried_posting_list, query_word_num);
    //convert the posting list to bitset
    bitset<MAX_DOC_ID> *serial_bitmap = new bitset<MAX_DOC_ID>[query_word_num];
    for (int i = 0; i < query_word_num; ++i) {
        for (int j = 0; j < queried_posting_list[i].len; ++j) {
            serial_bitmap[i].set(queried_posting_list[i].arr[j]);
        }
    }
    //get the intersection of the bitset by & to the first bitset
    gettimeofday(&tv_begin, nullptr);
    for (int i = 1; i < query_word_num; ++i) {
        serial_bitmap[0] &= serial_bitmap[i];
    }
    //convert the bitset to vector
    for (int i = 0; i <= max_possible_common_DocId; ++i) {
        if (serial_bitmap[0].test(i)) {
            serial_bitmap_result.push_back(i);
        }
    }
    gettimeofday(&tv_end, nullptr);
    time_serial_bitmap += (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;
    delete[]serial_bitmap;
}


void SIMD_bitmap_algorithm(POSTING_LIST *queried_posting_list, int query_word_num,
                           vector<unsigned int> &SIMD_bitmap_result) {
    //use NEON intrinsics to accelerate the bitmap algorithm
    //get the min max_DocID of the posting list as max possible docID the result intersection can be
    unsigned int max_possible_common_DocId = queried_posting_list[0].arr[queried_posting_list[0].len - 1];
    for (int i = 1; i < query_word_num; i++) {
        if (max_possible_common_DocId > queried_posting_list[i].arr[queried_posting_list[i].len - 1]) {
            max_possible_common_DocId = queried_posting_list[i].arr[queried_posting_list[i].len - 1];
        }
    }
    //convert the posting list to bitset
    bitset<EXPANDED_BITMAP_SIZE> *SIMD_bitmap = new bitset<EXPANDED_BITMAP_SIZE>[query_word_num];
    for (int i = 0; i < query_word_num; ++i) {
        for (int j = 0; j < queried_posting_list[i].len; ++j) {
            SIMD_bitmap[i].set(queried_posting_list[i].arr[j]);
        }
    }
    //get the intersection of the bitset by & to the first bitset
    gettimeofday(&tv_begin, NULL);
    //use NEON intrinsics to & the bitset
    //the pointer address must be stored in a long type,otherwise it will lose information! and it is address, not the pointer!
    //I checked this error for almost two hours...
    long SIMD_referring_bitmap_ptr = (long) &SIMD_bitmap[0];
    for (int i = 1; i < query_word_num; ++i) {
        long SIMD_bitmap_ptr = (long) &SIMD_bitmap[i];
        for (int j = 0; j < (((max_possible_common_DocId) / 128) + 1); j++) {
            unsigned int *SIMD_referring_bitmap_ptr_temp = (unsigned int *) (SIMD_referring_bitmap_ptr + 16 * j);
            unsigned int *SIMD_bitmap_ptr_temp = (unsigned int *) (SIMD_bitmap_ptr + 16 * j);
            uint32x4_t SIMD_referring_bitmap_temp = vld1q_u32(SIMD_referring_bitmap_ptr_temp);
            uint32x4_t SIMD_bitmap_temp = vld1q_u32(SIMD_bitmap_ptr_temp);
            SIMD_referring_bitmap_temp = vandq_u32(SIMD_referring_bitmap_temp, SIMD_bitmap_temp);
            vst1q_u32(SIMD_referring_bitmap_ptr_temp, SIMD_referring_bitmap_temp);
        }
    }
    //convert the bitset to vector
    for (int i = 0; i <= max_possible_common_DocId; ++i) {
        if (SIMD_bitmap[0].test(i)) {
            SIMD_bitmap_result.push_back(i);
        }
    }
    gettimeofday(&tv_end, NULL);
    time_SIMD_bitmap += (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;
    delete[]SIMD_bitmap;
}

void unrooled_bitmap_algorithm(POSTING_LIST *queried_posting_list, int query_word_num,
                               vector<unsigned int> &unrooled_bitmap_result) {
    unsigned int max_possible_common_DocId = queried_posting_list[0].arr[queried_posting_list[0].len - 1];
    for (int i = 1; i < query_word_num; i++) {
        if (max_possible_common_DocId > queried_posting_list[i].arr[queried_posting_list[i].len - 1]) {
            max_possible_common_DocId = queried_posting_list[i].arr[queried_posting_list[i].len - 1];
        }
    }
    //convert the posting list to bitset
    bitset<EXPANDED_BITMAP_SIZE> *unrolled_bitmap = new bitset<EXPANDED_BITMAP_SIZE>[query_word_num];
    for (int i = 0; i < query_word_num; ++i) {
        for (int j = 0; j < queried_posting_list[i].len; ++j) {
            unrolled_bitmap[i].set(queried_posting_list[i].arr[j]);
        }
    }
    //get the intersection of the bitset by & to the first bitset
    gettimeofday(&tv_begin, NULL);
    for (int i = 1; i < query_word_num; ++i) {
        for (int j = 0; j <= max_possible_common_DocId; j += 4) {
            unrolled_bitmap[0].set(j, unrolled_bitmap[0].test(j) & unrolled_bitmap[i].test(j));
            unrolled_bitmap[0].set(j + 1, unrolled_bitmap[0].test(j + 1) & unrolled_bitmap[i].test(j + 1));
            unrolled_bitmap[0].set(j + 2, unrolled_bitmap[0].test(j + 2) & unrolled_bitmap[i].test(j + 2));
            unrolled_bitmap[0].set(j + 3, unrolled_bitmap[0].test(j + 3) & unrolled_bitmap[i].test(j + 3));
        }
    }
    //convert the bitset to vector
    for (int i = 0; i <= max_possible_common_DocId; ++i) {
        if (unrolled_bitmap[0].test(i)) {
            unrooled_bitmap_result.emplace_back(i);
        }
    }
    gettimeofday(&tv_end, NULL);
    time_unrooled_bitmap += (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;
    delete[] unrolled_bitmap;

}

void blocked_bitmap_algorithm(POSTING_LIST *queried_posting_list, int query_word_num,
                              vector<unsigned int> &blocked_bitmap_result) {
    //divides the bitmap into blocks and records if the element in the blocks are all 0;
    //if all 0, then the block is useless and need not be &ed
    unsigned int max_possible_common_DocId = queried_posting_list[0].arr[queried_posting_list[0].len - 1];
    for (int i = 1; i < query_word_num; i++) {
        if (max_possible_common_DocId > queried_posting_list[i].arr[queried_posting_list[i].len - 1]) {
            max_possible_common_DocId = queried_posting_list[i].arr[queried_posting_list[i].len - 1];
        }
    }
    bitset<EXPANDED_BITMAP_SIZE> *block_bitmap = new bitset<EXPANDED_BITMAP_SIZE>[query_word_num];
    bool **block_bitmap_flag = new bool *[query_word_num];
    for (int i = 0; i < query_word_num; ++i) {
        block_bitmap_flag[i] = new bool[(max_possible_common_DocId / BLOCK_SIZE) + 1];
        for (int j = 0; j < (max_possible_common_DocId / BLOCK_SIZE) + 1; ++j) {
            block_bitmap_flag[i][j] = false;
        }
    }
    //convert the posting list to bitset
    for (int i = 0; i < query_word_num; ++i) {
        for (int j = 0; j < queried_posting_list[i].len; ++j) {
            block_bitmap[i].set(queried_posting_list[i].arr[j]);
            block_bitmap_flag[i][queried_posting_list[i].arr[j] / BLOCK_SIZE] = true;
        }
    }
    //get the intersection of the bitset by & to the first bitset
    gettimeofday(&tv_begin, nullptr);
    //start with the & of the boolean array,only if the two blocks are not all 0, then the & is needed
    //the & of all elements in the two blocks
    for (int i = 1; i < query_word_num; ++i) {
        for (int j = 0; j <= max_possible_common_DocId; j += BLOCK_SIZE) {
            if (block_bitmap_flag[0][j / BLOCK_SIZE]) {
                if (block_bitmap_flag[i][j / BLOCK_SIZE]) {
                    //the & of all elements in the two blocks
                    for (int k = 0; k < BLOCK_SIZE; ++k) {
                        block_bitmap[0].set(j + k, block_bitmap[0].test(j + k) & block_bitmap[i].test(j + k));
                    }
                } else {
                    //actually we can directly set the flag to false
                    block_bitmap_flag[0][j / BLOCK_SIZE] = false;
                }
            }
        }
    }
    //convert the bitset to vector
    //it is possible to have the intersection element only if the block flag inn the first bitset is true
    //start with detecting the block flag
    for (int i = 0; i < (max_possible_common_DocId / BLOCK_SIZE) + 1; i++) {
        if (block_bitmap_flag[0][i]) {
            //start with the & of the boolean array,only if the two blocks are not all 0, then the & is needed
            //the & of all elements in the two blocks
            for (int j = 0; j < BLOCK_SIZE; ++j) {
                if (block_bitmap[0].test(i * BLOCK_SIZE + j)) {
                    blocked_bitmap_result.emplace_back(i * BLOCK_SIZE + j);
                }
            }
        }
    }
    gettimeofday(&tv_end, nullptr);
    time_blocked_bitmap += (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;
    delete[]block_bitmap;
    for (int i = 0; i < query_word_num; ++i) {
        delete[]block_bitmap_flag[i];
    }
    delete[]block_bitmap_flag;
}

void SIMD_blocked_bitmap_algorithm(POSTING_LIST *queried_posting_list, int query_word_num,
                                   vector<unsigned int> &SIMD_blocked_bitmap_result) {
    //use NEON intrinsics to accelerate the & operation in the & process of blocks
    unsigned int max_possible_common_DocId = get_max_possible_common_DocId(queried_posting_list, query_word_num);
    bitset<EXPANDED_BITMAP_SIZE> *SIMD_block_bitmap = new bitset<EXPANDED_BITMAP_SIZE>[query_word_num];
        bool **SIMD_block_bitmap_flag = new bool *[query_word_num];
        for (int i = 0; i < query_word_num; ++i) {
            SIMD_block_bitmap_flag[i] = new bool[(max_possible_common_DocId / BLOCK_SIZE) + 1];
            for (int j = 0; j < (max_possible_common_DocId / BLOCK_SIZE) + 1; ++j) {
                SIMD_block_bitmap_flag[i][j] = false;
            }
        }
        //convert the posting list to bitset
        for (int i = 0; i < query_word_num; ++i) {
            for (int j = 0; j < queried_posting_list[i].len; ++j) {
                SIMD_block_bitmap[i].set(queried_posting_list[i].arr[j]);
                SIMD_block_bitmap_flag[i][queried_posting_list[i].arr[j] / BLOCK_SIZE] = true;
            }
        }
        //get the intersection of the bitset by & to the first bitset
        gettimeofday(&tv_begin, nullptr);
        long block_referring_bitmap_ptr = (long) &SIMD_block_bitmap[0];
        for (int i = 1; i < query_word_num; ++i) {
            long block_bitmap_ptr_i = (long) &SIMD_block_bitmap[i];
            for (int j = 0; j <= max_possible_common_DocId; j += BLOCK_SIZE) {
                if (SIMD_block_bitmap_flag[0][j / BLOCK_SIZE]) {
                    if (SIMD_block_bitmap_flag[i][j / BLOCK_SIZE]) {
                        //the & of all elements in the two blocks
                        //use the NEON intrinsics
                       for(int k=0;k<BLOCK_SIZE/128;k++)
                       {
                           //get the pointer that points to the start of the jth block
                           unsigned int *tmp_ptr_i = (unsigned int *) (block_bitmap_ptr_i + j/32 + 16*k);
                           unsigned int *tmp_ptr_0 = (unsigned int *) (block_referring_bitmap_ptr + j/32 + 16*k);
                           uint32x4_t tmp_i = vld1q_u32(tmp_ptr_i);
                           uint32x4_t tmp_0 = vld1q_u32(tmp_ptr_0);
                           tmp_0 = vandq_u32(tmp_i, tmp_0);
                           vst1q_u32(tmp_ptr_0, tmp_0);
                       }
                        for (int k = 0; k < BLOCK_SIZE; ++k) {
                            SIMD_block_bitmap[0].set(j + k, SIMD_block_bitmap[0].test(j + k) & SIMD_block_bitmap[i].test(j + k));
                        }

                    } else {
                        //actually we can directly set the flag to false
                        SIMD_block_bitmap_flag[0][j / BLOCK_SIZE] = false;
                    }
                }
            }
        }
        //convert the bitset to vector
        for (int i = 0; i < (max_possible_common_DocId / BLOCK_SIZE) + 1; i++) {
            if (SIMD_block_bitmap_flag[0][i]) {
                for (int j = 0; j < BLOCK_SIZE; ++j) {
                    if (SIMD_block_bitmap[0].test(i * BLOCK_SIZE + j)) {
                        SIMD_blocked_bitmap_result.emplace_back(i * BLOCK_SIZE + j);
                    }
                }
            }
        }
        gettimeofday(&tv_end, nullptr);
        time_SIMD_blocked_bitmap += (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;
        delete[]SIMD_block_bitmap;
        for(int i=0;i<query_word_num;i++)
        {
            delete[]SIMD_block_bitmap_flag[i];
        }
        delete[]SIMD_block_bitmap_flag;




}

void parallel_algorithm(int QueryNum, vector<vector<unsigned int>> &serial_result_container,
                        vector<vector<unsigned int>> &SIMD_result_container,
                        vector<vector<unsigned int>> &unrooled_result_container,
                        vector<vector<unsigned int>> &blocked_result_container,
                        vector<vector<unsigned int>> &SIMD_blocked_result_container) {

    for (int i = 0; i < QueryNum; ++i) {
        int query_word_num = query_list_container[i].size();
        POSTING_LIST *queried_posting_list = new POSTING_LIST[query_word_num];
        //get the posting list that the i-th query needs
        for (int j = 0; j < query_word_num; ++j) {
            int query_list_item = query_list_container[i][j];
            queried_posting_list[j] = posting_list_container[query_list_item];
        }
        vector<unsigned int> serial_bitmap_result;
        vector<unsigned int> SIMD_bitmap_result;
        vector<unsigned int> unrooled_bitmap_result;
        vector<unsigned int> blocked_bitmap_result;
        vector<unsigned int> SIMD_blocked_bitmap_result;

//     serial_bitmap_algorithm(queried_posting_list, query_word_num, serial_bitmap_result);
//        unrooled_bitmap_algorithm(queried_posting_list, query_word_num, unrooled_bitmap_result);
//        SIMD_bitmap_algorithm(queried_posting_list, query_word_num, SIMD_bitmap_result);
      blocked_bitmap_algorithm(queried_posting_list, query_word_num, blocked_bitmap_result);
//        SIMD_blocked_bitmap_algorithm(queried_posting_list, query_word_num, SIMD_blocked_bitmap_result);

        serial_result_container.push_back(serial_bitmap_result);
        SIMD_result_container.push_back(SIMD_bitmap_result);
        unrooled_result_container.push_back(unrooled_bitmap_result);
        blocked_result_container.push_back(blocked_bitmap_result);
        SIMD_blocked_result_container.push_back(SIMD_blocked_bitmap_result);

        serial_bitmap_result.clear();
        SIMD_bitmap_result.clear();
        unrooled_bitmap_result.clear();
        blocked_bitmap_result.clear();
        SIMD_blocked_bitmap_result.clear();
        delete[] queried_posting_list;
    }
}

int main() {
    if (read_posting_list() || read_query_list()) {
        printf("read_posting_list or read_query_list error!\n");
        return 1;
    } else {
        printf("read_posting_list and read_query_list success!\n");
        int QueryNum = 5;

        vector<vector<unsigned int>> serial_result;
        vector<vector<unsigned int>> SIMD_result;
        vector<vector<unsigned int>> unrooled_result;
        vector<vector<unsigned int>> blocked_result;
        vector<vector<unsigned int>> SIMD_blocked_result;
        parallel_algorithm(QueryNum, serial_result, SIMD_result, unrooled_result, blocked_result, SIMD_blocked_result);
        //test the correctness of the serial_result
        for (int i = 0; i < QueryNum; ++i) {
            printf("result %d: ", i);
            printf("%zu\n", serial_result[i].size());
            for (unsigned int j: serial_result[i]) {
                printf("%d ", j);
            }
            printf("\n");
        }
//        //test the correctness of the SIMD_result
//        for (int i = 0; i < QueryNum; ++i) {
//            printf("result %d: ", i);
//            printf("%zu\n", SIMD_result[i].size());
//            for (unsigned int j: SIMD_result[i]) {
//                printf("%d ", j);
//            }
//            printf("\n");
//        }
//        //test the correctness of the unrooled_result
//        for (int i = 0; i < QueryNum; ++i) {
//            printf("result %d: ", i);
//            printf("%zu\n", unrooled_result[i].size());
//            for (unsigned int j: unrooled_result[i]) {
//                printf("%d ", j);
//            }
//            printf("\n");
//        }
//        //test the correctness of the blocked_result
//        for (int i = 0; i < QueryNum; ++i) {
//            printf("result %d: ", i);
//            printf("%zu\n", blocked_result[i].size());
//            for (unsigned int j: blocked_result[i]) {
//                printf("%d ", j);
//            }
//            printf("\n");
//        }
        //test the correctness of the SIMD_blocked_result
        for (int i = 0; i < QueryNum; ++i) {
            printf("result %d: ", i);
            printf("%zu\n", SIMD_blocked_result[i].size());
            for (unsigned int j: SIMD_blocked_result[i]) {
                printf("%d ", j);
            }
            printf("\n");
        }
        printf("time_serial_bitmap:        %f\n", time_serial_bitmap);
        printf("time_SIMD_bitmap:          %f\n", time_SIMD_bitmap);
        printf("time_unrooled_bitmap:      %f\n", time_unrooled_bitmap);
        printf("time_blocked_bitmap:       %f\n", time_blocked_bitmap);
        printf("time_SIMD_blocked_bitmap:  %f\n", time_SIMD_blocked_bitmap);
        return 0;
    }

}



