/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

//定义了成功和错误的值
#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
//dict没有用到是，用来提示告警
#define DICT_NOTUSED(V) ((void) V)

//字典结构体，保存kv值的结构体
typedef struct dictEntry {
	//键
    void *key;
	//值
	union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
	//指向下个哈希表节点，形成链表
    struct dictEntry *next;
} dictEntry;

//字典类型
typedef struct dictType {
	//哈希值计算方法
    uint64_t (*hashFunction)(const void *key);
	//复制key方法
    void *(*keyDup)(void *privdata, const void *key);
	//复制val方法
	void *(*valDup)(void *privdata, const void *obj);
	//比较key
	int (*keyCompare)(void *privdata, const void *key1, const void *key2);
	//销毁键的函数
    void (*keyDestructor)(void *privdata, void *key);
	//销毁值的函数
	void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
//哈希表
typedef struct dictht {
	//哈希表数组
    dictEntry **table;
	//哈希表的大小
	unsigned long size;
	//哈希表大小掩码，用来计算索引值
	//总是等于size-1
    unsigned long sizemask;
	//该哈希表已有的节点的数量
    unsigned long used;
} dictht;

//字典主操作类
typedef struct dict {
	//字典的类型
    dictType *type;
	//私有数据指针
    void *privdata;
	//双哈希表
    dictht ht[2];
	//重定位哈希表的标志
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
	//当前迭代器的数量
    unsigned long iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
//字典的迭代器
//如果这是一个安全的迭代器，那么可以调用函数，否则只能用来遍历
typedef struct dictIterator {
	//字典操作类指针
    dict *d;
	//下标
    long index;
    int table, safe;
	//字典实体指针
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

//字典扫描方法
typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
//初始化哈希表的个数
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
//字典释放val函数时候调用，如果dict中的dictType定义了这个函数指针
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

//字典val函数复制时候调用
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

//设置dictEntry中共用体v中有符号类型的值
#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

//设置dictEntry中共用体v中无符号类型的值
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

//设置dictEntry中共用体v中double类型的值
#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

//调用dictType定义的key析构函数
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

//调用dictType定义的key复制函数，没有定义直接赋值
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

//调用dictType定义的key比较函数，没有定义直接key值直接比较
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

//哈希定位方法
#define dictHashKey(d, key) (d)->type->hashFunction(key)
//获取key值
#define dictGetKey(he) ((he)->key)
//获取val值
#define dictGetVal(he) ((he)->v.val)
//获取有符号值
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
//获取无符号值
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
//获取double值
#define dictGetDoubleVal(he) ((he)->v.d)
//获取dict字典中表的大小
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
//获取dict字典中总的表的总正在被使用的数量 
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
//字典有无被重定位过 
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
//创建字典主操作类
dict *dictCreate(dictType *type, void *privDataPtr);
//字典扩展
int dictExpand(dict *d, unsigned long size);
//增加字典的节点
int dictAdd(dict *d, void *key, void *val);
//字典添加一个只有key值的dicEntry
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);
//
dictEntry *dictAddOrFind(dict *d, void *key);
//替代dict中一个字典集
int dictReplace(dict *d, void *key, void *val);
//根据key删除一个字典集
int dictDelete(dict *d, const void *key);
//
dictEntry *dictUnlink(dict *ht, const void *key);

void dictFreeUnlinkedEntry(dict *d, dictEntry *he);
//释放整个字典
void dictRelease(dict *d);
//查找键值所在的字典
dictEntry * dictFind(dict *d, const void *key);
//
void *dictFetchValue(dict *d, const void *key);
//重新计算大小
int dictResize(dict *d);
//得到字典的迭代器
dictIterator *dictGetIterator(dict *d);
//得到字典的安全迭代器
dictIterator *dictGetSafeIterator(dict *d);
//得到下一个字典
dictEntry *dictNext(dictIterator *iter);
//释放字典的迭代器
void dictReleaseIterator(dictIterator *iter);
//随机获取一个字典
dictEntry *dictGetRandomKey(dict *d);
//
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
//
void dictGetStats(char *buf, size_t bufsize, dict *d);
//输入的key值，目标长度，此方法帮你计算出索引值
uint64_t dictGenHashFunction(const void *key, int len);
//这里提供了一种比较简单的哈希算法
uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);
//清空字典
void dictEmpty(dict *d, void(callback)(void*));
//启用调整方法
void dictEnableResize(void);
//禁用调整方法  
void dictDisableResize(void);
///hash重定位，主要从旧的表映射到新表中,分n轮定位
int dictRehash(dict *d, int n);
//在给定时间内，循环执行哈希重定位
int dictRehashMilliseconds(dict *d, int ms);
//设置哈希方法种子
void dictSetHashFunctionSeed(uint8_t *seed);
//获取哈希种子
uint8_t *dictGetHashFunctionSeed(void);
//字典扫描方法
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
//
unsigned int dictGetHash(dict *d, const void *key);
//
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, unsigned int hash);

/* Hash table types */
//哈希表的类型
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
