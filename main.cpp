#include <stdio.h>
#include<vector>
#include<iostream>
#include <cstdlib>

using namespace std;
FILE *fi;
FILE *fq;
struct POSTING_LIST {
    unsigned int  len;
    unsigned int *arr;
}*posting_list_container;

const int POSTING_LIST_NUM = 1756;
unsigned int i, array_len;
unsigned int *temp_arr;
int posting_list_counter, n,query_list_count;
vector<vector<int> > query_list_container;



int binary_search(POSTING_LIST *list, unsigned int element, int index)
{
    int low=index, high=list->len-1, mid;
    while(low<=high)
    {
        mid = (low+high)/2;
        if(list->arr[mid]==element)
            return mid;
        else if(list->arr[mid]<element)
            low=mid+1;
        else
            high=mid-1;
    }
    return -1;

}

void simplified_Adp_body(POSTING_LIST *qpl, int query_list_len, vector<unsigned int> &result_list){
    bool flag;
    unsigned int key_element;
    vector<int> finding_pointer(query_list_len,0);
    for (int k=0; k < qpl[0].len; k++)
    {
        flag=true;
        key_element=qpl[0].arr[k];
        for (int m = 1; m < query_list_len; ++m) {
            //如果当前的关键元素比某一个倒排链表的最后一个元素都大，后面的元素肯定不会是倒排链表的交集，直接返回结果
//            if (key_element>qpl[m].arr[qpl[m].len-1])
//            {
//                return result_list;
//            }
//            cout<<m;
            int location=binary_search(&qpl[m], key_element, finding_pointer[m]);
            if (location==-1)
            {
                flag=false;
                break;
            }
            else
            {
                finding_pointer[m]=location;
            }
        }
        if (flag)
        {
            result_list.push_back(key_element);
        }
    }
}

void simplified_Adp(int QueryNum,vector<vector<unsigned int>> &result_list_container)
{
    for (int i = 0; i < QueryNum; ++i) {
        int query_list_len = query_list_container[i].size();
        printf("query %d len:%d\n",i,query_list_len);
        POSTING_LIST *queried_posting_list = new POSTING_LIST[query_list_len];

        for (int j = 0; j < query_list_len; ++j) {
            int query_list_item = query_list_container[i][j];
            queried_posting_list[j] = posting_list_container[query_list_item];
        }
        vector<unsigned int> result_list;
        simplified_Adp_body(queried_posting_list, query_list_len, result_list);
        result_list_container.push_back(result_list);
        result_list.clear();
        delete[] queried_posting_list;
    }
}



vector<int> to_int_list(char* line) {
    vector<int> int_list;
    int i = 0;
    int num = 0;
    while (line[i] == ' '||(line[i]>=48&&line[i]<=57)) {
        num = 0;
        while(line[i] != ' ') {
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
    posting_list_container = (struct POSTING_LIST *)malloc(POSTING_LIST_NUM * sizeof(struct POSTING_LIST));
    if (NULL == posting_list_container) {
        printf("Can not malloc enough space\n");
        return 2;
    }
    while (1) {
        fread(&array_len, sizeof(unsigned int), 1, fi);
        if (feof(fi)) break;
        temp_arr = (unsigned int *)malloc(array_len * sizeof(unsigned int));
        for (i = 0; i < array_len; i++) {
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
    vector<int> query_list;
    char* line = new char[100];
    while ((fgets(line, 100, fq)) != NULL)
    {
        query_list = to_int_list(line);
        query_list_container.push_back(query_list);
        query_list_count++;
    }
    fclose(fq);

    printf("query_list num: %d\n", query_list_count);
    return 0;
}

int main() {

    if(read_posting_list()||read_query_list())
    {
        printf("read_posting_list or read_query_list error!\n");
        return 1;
    }
    else
    {

        printf("read_posting_list and read_query_list success!\n");
        int QueryNum = 5;
        vector<vector<unsigned int>> test_simplified_Adp_result;
        simplified_Adp(QueryNum,test_simplified_Adp_result);
        for (int j = 0; j < QueryNum; ++j) {
            printf("result %d: ", j);
            printf("%d\n",test_simplified_Adp_result[j].size());
            for (int k = 0; k < test_simplified_Adp_result[j].size(); ++k) {
                printf("%d ",test_simplified_Adp_result[j][k]);
            }
            printf("\n");
        }

        free(posting_list_container);
        return 0;


    }

}




