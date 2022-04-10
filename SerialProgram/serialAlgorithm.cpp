#include <stdio.h>
#include<vector>
#include<sys/time.h>
#include<time.h>
#include<iostream>
#include <cstdlib>

using namespace std;
FILE *fi;
FILE *fq;
struct POSTING_LIST {
    unsigned int len;
    unsigned int *arr;
} *posting_list_container;

const int POSTING_LIST_NUM = 1756;
unsigned int array_len;
unsigned int *temp_arr;
int posting_list_counter, query_list_count;
vector<vector<int> > query_list_container;
double timeSvS = 0;
double timeSvS_refine = 0;
double timeSimplified_Adp = 0;
double timeAdp = 0;
timeval tv_begin, tv_end;
struct timespec sts,ets;
long long time_use;


int binary_search(POSTING_LIST *list, unsigned int element, int index) {
    int low = index, high = list->len - 1, mid;
    while (low <= high) {
        mid = (low + high) / 2;
        if (list->arr[mid] == element)
            return mid;
        else if (list->arr[mid] < element)
            low = mid + 1;
        else
            high = mid - 1;
    }
    return -1;

}

int binary_search_with_position(POSTING_LIST *list, unsigned int element, int index) {
    //如果找到返回该元素位置，否则返回不小于它的第一个元素的位置
    int low = index, high = list->len - 1, mid;
    while (low <= high) {
        mid = (low + high) / 2;
        if (list->arr[mid] == element)
            return mid;
        else if (list->arr[mid] < element)
            low = mid + 1;
        else
            high = mid - 1;
    }
    return low;
}

void get_sorted_index(POSTING_LIST *queried_posting_list, int query_word_num, int *sorted_index) {

    for (int i = 0; i < query_word_num; i++) {
        sorted_index[i] = i;
    }
    for (int i = 0; i < query_word_num - 1; i++) {
        for (int j = i + 1; j < query_word_num; j++) {
            if (queried_posting_list[sorted_index[i]].len > queried_posting_list[sorted_index[j]].len) {
                int temp = sorted_index[i];
                sorted_index[i] = sorted_index[j];
                sorted_index[j] = temp;
            }
        }
    }
}

void get_sorted_index_Adp( POSTING_LIST *queried_posting_list, int query_word_num, int *sorted_index, vector<int> &finding_pointer) {
    //为Adp算法进行的重载，按当前实际还需要查询的元素的多少排序
    for (int i = 0; i < query_word_num; i++) {
        sorted_index[i] = i;
    }
    for (int i = 0; i < query_word_num - 1; i++) {
        for (int j = i + 1; j < query_word_num; j++) {
            if (queried_posting_list[sorted_index[i]].len-finding_pointer[sorted_index[i]] > queried_posting_list[sorted_index[j]].len-finding_pointer[sorted_index[j]]) {
                int temp = sorted_index[i];
                sorted_index[i] = sorted_index[j];
                sorted_index[j] = temp;
            }
        }
    }
}


void SvS(POSTING_LIST *queried_posting_list, int query_word_num, vector<unsigned int> &result_list) {
    //两表求交，求交结果再与其他表求交
    //先对倒排链表的长度进行排序，由于查询的关键字一般只有几个，使用普通的选择排序即可
    //同时，排序的过程也是算法的一部分，也需要计入时间
    int *sorted_index = new int[query_word_num];
    get_sorted_index(queried_posting_list, query_word_num, sorted_index);
    //将最短的倒排链表放在最前面
    for (int k = 0; k < queried_posting_list[sorted_index[0]].len; k++) {
        result_list.push_back(queried_posting_list[sorted_index[0]].arr[k]);
    }
    for (int i = 1; i < query_word_num; i++) {
        vector<unsigned int> temp_result_list;

        for (int j = 0; j < result_list.size(); j++) {
            if (binary_search(&queried_posting_list[sorted_index[i]], result_list[j], 0) != -1) {
                temp_result_list.push_back(result_list[j]);
            }
        }
        result_list = temp_result_list;
    }

}

void simplified_Adp(POSTING_LIST *queried_posting_list, int query_word_num, vector<unsigned int> &result_list) {
    //不对列表长度进行动态更新的Adp算法
    //同样先对倒排列表按长度进行排序
    int *sorted_index = new int[query_word_num];
    get_sorted_index(queried_posting_list, query_word_num, sorted_index);
    bool flag;
    unsigned int key_element;
    vector<int> finding_pointer(query_word_num, 0);
    //每次从最短的倒排索引表中选元素

    for (int k = 0; k < queried_posting_list[sorted_index[0]].len; k++) {
        flag = true;
        key_element = queried_posting_list[sorted_index[0]].arr[k];
        for (int m = 1; m < query_word_num; ++m) {
            int mth_short=sorted_index[m];
            POSTING_LIST searching_list=queried_posting_list[mth_short];
            //如果当前的关键元素比某一个倒排链表的最后一个元素都大，后面的元素肯定不会是倒排链表的交集，直接返回结果
            if (key_element>searching_list.arr[searching_list.len-1])
            {
                goto end;
            }
            int location = binary_search_with_position(&queried_posting_list[mth_short], key_element, finding_pointer[sorted_index[m]]);
            if (searching_list.arr[location] != key_element) {
                flag = false;
                break;
            }
            finding_pointer[mth_short] = location;
        }
        if (flag) {
            result_list.push_back(key_element);
        }
    }
    end: delete[] sorted_index;
}


void SvS_refine(POSTING_LIST *queried_posting_list, int query_word_num, vector<unsigned int> &result_list) {
    //每次查找到元素后记录下该元素的位置，下次查找时从该位置开始查找
    int *sorted_index = new int[query_word_num];
    get_sorted_index(queried_posting_list, query_word_num, sorted_index);
    vector<int> finding_pointer(query_word_num, 0);
    for (int i = 0; i < queried_posting_list[sorted_index[0]].len; i++) {
        result_list.push_back(queried_posting_list[sorted_index[0]].arr[i]);
    }
    for (int i = 1; i < query_word_num; i++) {
        vector<unsigned int> temp_result_list;
        for (unsigned int & j : result_list) {
            int location = binary_search_with_position(&queried_posting_list[sorted_index[i]], j, finding_pointer[i]);
            if (queried_posting_list[sorted_index[i]].arr[location]== j) {
                temp_result_list.push_back(j);
            }
            finding_pointer[sorted_index[i]] = location;
        }

        result_list = temp_result_list;
    }

    delete[] sorted_index;

}

void Adp(POSTING_LIST *queried_posting_list, int query_word_num, vector<unsigned int> &result_list) {
    //每次搜索的过程中发现了不匹配，都按照每一个表内剩余元素的个数重新排序
    vector<int> finding_pointer(query_word_num, 0);
    int *sorted_index = new int[query_word_num];
    unsigned int key_element;
    //实际上只需要对长度记录表初始化一次
    for (int i = 0; i < query_word_num; i++) {
        sorted_index[i] = i;
    }
    bool flag;
    while(1) {
        get_sorted_index_Adp(queried_posting_list, query_word_num, sorted_index, finding_pointer);
        key_element = queried_posting_list[sorted_index[0]].arr[finding_pointer[sorted_index[0]]];
        flag = true;
        for (int m = 1; m < query_word_num; ++m) {
            int mth_short = sorted_index[m];
            POSTING_LIST searching_list = queried_posting_list[mth_short];
            int location = binary_search_with_position(&queried_posting_list[mth_short], key_element,
                                                       finding_pointer[sorted_index[m]]);
            if (searching_list.arr[location] != key_element) {
                if (searching_list.len==location) {
                    //最后一个元素都不是的话，那么这张表就被找完了，算法终止
                    goto end_Adp;
                }
                flag = false;
                break;
            }
            finding_pointer[mth_short] = location;
        }
        if (flag) {
            result_list.push_back(key_element);
        }
        finding_pointer[sorted_index[0]]++;
        if(finding_pointer[sorted_index[0]]==queried_posting_list[sorted_index[0]].len)
        {
            //如果当前最短的倒排链表搜索完了，算法也停止
            goto end_Adp;
        }
    }
    end_Adp:delete[] sorted_index;
}

void serial_algorithms(int QueryNum, vector<vector<unsigned int>> &SvS_result_container,
                       vector<vector<unsigned int>> &SvS_refine_result_container,
                       vector<vector<unsigned int>> &simplified_Adp_result_container, vector<vector<unsigned int>> &Adp_result_container) {

    for (int i = 0; i < QueryNum; ++i) {
        int query_word_num = query_list_container[i].size();
        POSTING_LIST *queried_posting_list = new POSTING_LIST[query_word_num];
        //获取第i次查询需要的倒排链表
        for (int j = 0; j < query_word_num; ++j) {
            int query_list_item = query_list_container[i][j];
            queried_posting_list[j] = posting_list_container[query_list_item];
        }
        //使用不同的算法获取第i次查询的结果
        vector<unsigned int> simplified_Adp_result_list;
        vector<unsigned int> SvS_result_list;
        vector<unsigned int> SvS_refine_result_list;
        vector<unsigned int> Adp_result_list;

        //简化Adp算法计时
        gettimeofday(&tv_begin, NULL);
        simplified_Adp(queried_posting_list, query_word_num, simplified_Adp_result_list);
        gettimeofday(&tv_end, NULL);

        //简化Adp算法，高精度计时
        timespec_get(&sts, TIME_UTC);
        simplified_Adp(queried_posting_list, query_word_num, simplified_Adp_result_list);
        timespec_get(&ets, TIME_UTC);
        time_t dsec=ets.tv_sec-sts.tv_sec;
        long dnsec=ets.tv_nsec - sts.tv_nsec;
        if (dnsec<0){
            dsec--;
            dnsec+=1000000000ll;
        }
        time_use+= 1000000000 * (dsec) + dnsec;
        timeSimplified_Adp += (tv_end.tv_sec - tv_begin.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;

        //Adp算法计时
        gettimeofday(&tv_begin, NULL);
        Adp(queried_posting_list, query_word_num, Adp_result_list);
        gettimeofday(&tv_end, NULL);
        timeAdp+=(tv_end.tv_sec - tv_begin.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;

        //SvS算法计时
        gettimeofday(&tv_begin, NULL);
        SvS(queried_posting_list, query_word_num, SvS_result_list);
        gettimeofday(&tv_end, NULL);
        timeSvS += (tv_end.tv_sec - tv_begin.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;

        //改进SvS算法计时
        gettimeofday(&tv_begin, NULL);
        SvS_refine(queried_posting_list, query_word_num, SvS_refine_result_list);
        gettimeofday(&tv_end, NULL);
        timeSvS_refine += (tv_end.tv_sec - tv_begin.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_begin.tv_usec) / 1000.0;
        //保存结果
        simplified_Adp_result_container.push_back(simplified_Adp_result_list);
        SvS_result_container.push_back(SvS_result_list);
        SvS_refine_result_container.push_back(SvS_refine_result_list);
        Adp_result_container.push_back(Adp_result_list);
        //清空该次搜索结果的容器，以备下一次使用
        simplified_Adp_result_list.clear();
        SvS_result_list.clear();
        SvS_refine_result_list.clear();
        Adp_result_list.clear();
        delete[] queried_posting_list;
    }
}
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


int main() {

    if (read_posting_list() || read_query_list()) {
        printf("read_posting_list or read_query_list error!\n");
        return 1;
    } else {

        printf("read_posting_list and read_query_list success!\n");
        int QueryNum = 100;
        vector<vector<unsigned int>> SvS_result;
        vector<vector<unsigned int>> SvS_refine_result;
        vector<vector<unsigned int>> simplified_Adp_result;
        vector<vector<unsigned int>> Adp_result;
        serial_algorithms(QueryNum, SvS_result, SvS_refine_result, simplified_Adp_result, Adp_result);

//        //测试simplified_Adp算法正确性
//        for (int j = 0; j < QueryNum; ++j) {
//            printf("result %d: ", j);
//            printf("%d\n", simplified_Adp_result[j].size());
////            for (int k = 0; k < simplified_Adp_result[j].size(); ++k) {
////                printf("%d ", simplified_Adp_result[j][k]);
////            }
////            printf("\n");
//        }
//
//        //测试SvS算法正确性
//        for (int j = 0; j < QueryNum; ++j) {
//            printf("result %d: ", j);
//            printf("%d\n", SvS_result[j].size());
////            for (int k = 0; k < SvS_result[j].size(); ++k) {
////                printf("%d ", SvS_result[j][k]);
////            }
////            printf("\n");
//        }
//        //测试SvS_refine算法正确性
//        for (int j = 0; j < QueryNum; ++j) {
//            printf("result %d: ", j);
//            printf("%d\n", SvS_refine_result[j].size());
////            for (int k = 0; k < SvS_refine_result[j].size(); ++k) {
////                printf("%d ", SvS_refine_result[j][k]);
////            }
////            printf("\n");
//        }
//        //测试Adp算法正确性
//        for (int j = 0; j < QueryNum; ++j) {
//            printf("result %d: ", j);
//            printf("%d\n", Adp_result[j].size());
////            for (int k = 0; k < Adp_result[j].size(); ++k) {
////                printf("%d ", Adp_result[j][k]);
////            }
////            printf("\n");
//        }
        printf("SvS time:            %f ms\n", timeSvS);
        printf("SvS_refine time:     %f ms\n", timeSvS_refine);
        printf("simplified_Adp time: %f ms\n", timeSimplified_Adp);
        printf("Adp time:            %f ms\n", timeAdp);
        printf("simplified_Adp time: %lld ns (high precise)\n",time_use);
        free(posting_list_container);
        return 0;
    }
}




