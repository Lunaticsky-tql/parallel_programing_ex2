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
timeval tv_begin, tv_end;


const int POSTING_LIST_NUM = 1756;
unsigned int array_len;
unsigned int *temp_arr;
const unsigned int MAX_DOC_ID=25205174;
vector<vector<int> > query_list_container;
int posting_list_counter, query_list_count;


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

void serial_bitmap_algorithm(POSTING_LIST *queried_posting_list, int query_word_num, vector<unsigned int> &serial_bitmap_result) {
    //use bitset to store the posting list
    //get the min max_DocID of the posting list as max possible docID the result intersection can be
    unsigned int max_possible_common_DocId = queried_posting_list[0].arr[queried_posting_list[0].len - 1];
    for (int i = 1; i < query_word_num; i++) {
        if (max_possible_common_DocId > queried_posting_list[i].arr[queried_posting_list[i].len - 1]) {
            max_possible_common_DocId = queried_posting_list[i].arr[queried_posting_list[i].len - 1];
        }
    }
    //convert the posting list to bitset
    bitset<MAX_DOC_ID> *serial_bitmap = new bitset<MAX_DOC_ID>[query_word_num];
    for (int i = 0; i < query_word_num; ++i) {
        for (int j = 0; j < queried_posting_list[i].len; ++j) {
            serial_bitmap[i].set(queried_posting_list[i].arr[j]);
        }
    }
    //get the intersection of the bitset by & to the first bitset
    gettimeofday(&tv_begin, NULL);
    for (int i = 1; i < query_word_num; ++i) {
        serial_bitmap[0] &= serial_bitmap[i];
    }
    //convert the bitset to vector
    for (int i = 0; i <= max_possible_common_DocId; ++i) {
        if (serial_bitmap[0].test(i)) {
            serial_bitmap_result.push_back(i);
        }
    }
    gettimeofday(&tv_end, NULL);
    time_serial_bitmap+= (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;
}


void SIMD_bitmap_algorithm(POSTING_LIST *queried_posting_list, int query_word_num, vector<unsigned int> &SIMD_bitmap_result) {
    //use NEON intrinsics to accelerate the bitmap algorithm
    //get the min max_DocID of the posting list as max possible docID the result intersection can be
    unsigned int max_possible_common_DocId = queried_posting_list[0].arr[queried_posting_list[0].len - 1];
    for (int i = 1; i < query_word_num; i++) {
        if (max_possible_common_DocId > queried_posting_list[i].arr[queried_posting_list[i].len - 1]) {
            max_possible_common_DocId = queried_posting_list[i].arr[queried_posting_list[i].len - 1];
        }
    }
    //convert the posting list to bitset



}

void parallel_algorithm(int QueryNum, vector<vector<unsigned int>> &serial_bitmap_result_container, vector<vector<unsigned int>> &SIMD_bitmap_result_container) {

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

        serial_bitmap_algorithm(queried_posting_list, query_word_num, serial_bitmap_result);
//        SIMD_bitmap_algorithm(queried_posting_list, query_word_num, SIMD_bitmap_result);

        serial_bitmap_result_container.push_back(serial_bitmap_result);

    }
}

int main()
{
    if (read_posting_list() || read_query_list()) {
        printf("read_posting_list or read_query_list error!\n");
        return 1;
    } else{
        printf("read_posting_list and read_query_list success!\n");
        int QueryNum = 5;
        vector<vector<unsigned int>> serial_bitmap_result;
        vector<vector<unsigned int>> SIMD_bitmap_result;
        parallel_algorithm(QueryNum, serial_bitmap_result, SIMD_bitmap_result);
        //test the correctness of the serial_bitmap_result
        for(int i=0;i<QueryNum;++i){
            printf("result %d: ", i);
            printf("%d\n", serial_bitmap_result[i].size());
            for(int j=0;j<serial_bitmap_result[i].size();++j){
                printf("%d ", serial_bitmap_result[i][j]);
            }
            printf("\n");
        }
        printf("time_serial_bitmap: %f\n", time_serial_bitmap);
        return 0;
    }

}



