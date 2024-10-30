#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct CacheNode {
    char *path, *data;      // key: value = path: data
    size_t size;            // 캐시된 데이터 크기
    time_t timestamp;       // 캐시된 시간 (유효성 검사)
    struct CacheNode *next; // 연결리스트 다음 노드 포인터
} CacheNode;

typedef struct Cache {
    CacheNode *head;        // 시작 노드
    size_t curSize;         // 연결리스트 사이즈
} Cache;


Cache* createCache();
void freeCache(Cache *cache);

CacheNode* createCacheNode(const char *path, const char *data, size_t size);
void evictOldCache(Cache *cache);

CacheNode* readCache(Cache *cache, const char *path);
void writeCache(Cache *cache, const char *path, const char *data, size_t size);
void sendCache(CacheNode *node);

/* proxy가 Cache에서 데이터 가져가면, 그 데이터가 있는 CacheNode는 가장 마지막 노드로 보내기(LRU) */
CacheNode* readCache(Cache *cache, const char *path) {
    
}

/* proxy가 Cache에 새로운 데이터 추가하려고 하면, 가장 마지막 노드에 새 cacheNode 추가하기  */
void writeCache(Cache *cache, const char *path, const char *data, size_t size) {

}

/* client에게 Cache에 있는 데이터 전송 */
void sendCache(CacheNode *node) {
    
}

/* Cache 초기화 */
Cache* createCache() {
    Cache *cache = malloc(sizeof(Cache));
    cache->head = NULL;
    cache->curSize = 0;
    return cache;
}

/* Cache 구조체 해제 */
void freeCache(Cache *cache) {
    CacheNode *cur = cache->head;
    while (cur) {
        CacheNode *temp = cur;
        cur = cur->next;
        free(temp->path);
        free(temp->data);
        free(temp);
    }
    free(cache);
}


CacheNode* createCacheNode(const char *path, const char *data, size_t size) {

}

/* 가장 첫번째 cacheNode 삭제(LRU) */
void evictOldCache(Cache *cache) {

}