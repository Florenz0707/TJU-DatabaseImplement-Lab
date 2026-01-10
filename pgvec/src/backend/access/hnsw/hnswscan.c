#include "postgres.h"

#include "access/relscan.h"
#include "access/hnsw.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/float.h"
#include "utils/memutils.h"

/*
 * Algorithm 5 from paper
 */
static List *
GetScanItems(IndexScanDesc scan, Datum value)
{
	HnswScanOpaque so = (HnswScanOpaque)scan->opaque;
	Relation index = scan->indexRelation;
	HnswSupport *support = &so->support;
	List *ep;
	List *w;
	int m;
	HnswElement entryPoint;
	char *base = NULL;
	HnswQuery *q = &so->q;

	/* Get m and entry point */
	HnswGetMetaPageInfo(index, &m, &entryPoint);

	q->value = value;
	so->m = m;

	if (entryPoint == NULL)
		return NIL;

	ep = list_make1(HnswEntryCandidate(base, entryPoint, q, index, support, false));

	/* 题目1：补全HNSW从最高层到第1层的完整层级遍历逻辑
	 * 核心提示：
	 * 1. 循环变量lc从entryPoint->level开始，递减到1（包含1）
	 * 2. 循环体内调用HnswSearchLayer
	 * 3. 每次调用后将返回值赋值给w，并更新ep = w
	 */
	/******** 填空开始 ********/
	for (int lc = entryPoint->level; lc >= 1; lc--)
	{
		w = HnswSearchLayer(base, q, ep, 1, lc, index, support, m, false, NULL, NULL, NULL, true, NULL);
		ep = w;
	}
	/******** 填空结束 ********/

	/* 题目2：补全底层（level=0）搜索的完整HnswSearchLayer调用逻辑
	 * 核心提示：
	 * 1. 直接return HnswSearchLayer的调用结果
	 * 2. 其余参数复用当前函数内的base/q/ep/index/support/m
	 */
	/******** 填空开始 ********/
	return HnswSearchLayer(base, q, ep, hnsw_ef_search, 0, index, support, m, false, NULL, &so->v, hnsw_iterative_scan != HNSW_ITERATIVE_SCAN_OFF ? &so->discarded : NULL, true, &so->tuples);
	/******** 填空结束 ********/
}

/*
 * Resume scan at ground level with discarded candidates
 */
static List *
ResumeScanItems(IndexScanDesc scan)
{
	HnswScanOpaque so = (HnswScanOpaque)scan->opaque;
	Relation index = scan->indexRelation;
	List *ep = NIL;
	char *base = NULL;
	int batch_size = hnsw_ef_search;

	HnswSearchCandidate *sc;

	if (pairingheap_is_empty(so->discarded))
		return NIL;

	/* 题目3：补全获取下一批候选集的完整循环逻辑
	 * 核心提示：
	 * 1. 循环目的：从discarded堆中取最多batch_size个候选，追加到ep列表
	 * 2. 循环范围：i从0到batch_size-1
	 * 3. 循环体步骤：
	 *    a. 定义HnswSearchCandidate* sc变量
	 *    b. 判空：若discarded堆为空则break
	 *    c. 取堆顶：pairingheap_remove_first(so->discarded) + HnswGetSearchCandidate(w_node, ...)
	 *    d. 追加：lappend(ep, sc)
	 */
	/******** 填空开始 ********/
	for (int i = 0; i < batch_size; i++)
	{
		if (pairingheap_is_empty(so->discarded))
			break;

		sc = HnswGetSearchCandidate(w_node, pairingheap_remove_first(so->discarded));
		ep = lappend(ep, sc);
	}
	/******** 填空结束 ********/

	return HnswSearchLayer(base, &so->q, ep, batch_size, 0, index, &so->support, so->m, false, NULL, &so->v, &so->discarded, false, &so->tuples);
}

/*
 * Get scan value
 */
static Datum
GetScanValue(IndexScanDesc scan)
{
	HnswScanOpaque so = (HnswScanOpaque)scan->opaque;
	Datum value;

	/* 题目4：补全GetScanValue的完整逻辑
	 * 核心提示：
	 * 1. 分支1（NULL值）：scan->orderByData->sk_flags包含SK_ISNULL时，value=PointerGetDatum(NULL)
	 * 2. 分支2（非NULL值）：
	 *    a. 基础赋值
	 *    b. 断言校验
	 *    c. 归一化
	 */
	/******** 填空开始 ********/
	if (scan->orderByData->sk_flags & SK_ISNULL)
		value = PointerGetDatum(NULL);
	else
	{
		value = scan->orderByData->sk_argument;

		/* Value should not be compressed or toasted */
		Assert(!VARATT_IS_COMPRESSED(DatumGetPointer(value)));
		Assert(!VARATT_IS_EXTENDED(DatumGetPointer(value)));

		/* Normalize if needed */
		if (so->support.normprocinfo != NULL)
			value = HnswNormValue(so->typeInfo, so->support.collation, value);
	}
	/******** 填空结束 ********/

	return value;
}

#if defined(HNSW_MEMORY)
/*
 * Show memory usage
 */
static void
ShowMemoryUsage(HnswScanOpaque so)
{
	/* 题目5：补全ShowMemoryUsage的完整实现
	 * 核心提示：
	 * 1. 功能：打印内存使用量（KB）和已扫描元组数
	 * 2. 实现：调用elog(INFO, ...)，格式化字符串包含：
	 *    - "memory: %zu KB, tuples: " + INT64_FORMAT（PostgreSQL 64位整数宏）
	 *    - 内存计算：MemoryContextMemAllocated(so->tmpCtx, false) / 1024（字节转KB）
	 *    - 元组数：so->tuples
	 */
	/******** 填空开始 ********/
	elog(INFO, "memory: %zu KB, tuples: " INT64_FORMAT,
		 MemoryContextMemAllocated(so->tmpCtx, false) / 1024,
		 so->tuples);
	/******** 填空结束 ********/
}
#endif

/*
 * Prepare for an index scan
 */
IndexScanDesc
hnswbeginscan(Relation index, int nkeys, int norderbys)
{
	IndexScanDesc scan;
	HnswScanOpaque so;
	double maxMemory;

	scan = RelationGetIndexScan(index, nkeys, norderbys);

	so = (HnswScanOpaque)palloc(sizeof(HnswScanOpaqueData));
	so->typeInfo = HnswGetTypeInfo(index);

	/* Set support functions */
	HnswInitSupport(&so->support, index);

	/* 题目6：补全临时内存上下文创建+最大内存计算的完整逻辑
	 * 核心提示：
	 * 1. 创建内存上下文：AllocSetContextCreate(父上下文/名称/0/8*1024/256*1024)，赋值给so->tmpCtx
	 * 2. 计算最大内存：
	 *    a. 公式：maxMemory = (double)work_mem * hnsw_scan_mem_multiplier * 1024 + 256
	 *    b. 限制上限：so->maxMemory = Min(maxMemory, (double)SIZE_MAX)
	 */
	/******** 填空开始 ********/
	so->tmpCtx = AllocSetContextCreate(CurrentMemoryContext,
									   "HNSW scan temporary context",
									   0,
									   8 * 1024,
									   256 * 1024);
	maxMemory = (double)work_mem * hnsw_scan_mem_multiplier * 1024.0 + 256;
	so->maxMemory = Min(maxMemory, (double)SIZE_MAX);
	/******** 填空结束 ********/

	scan->opaque = so;

	return scan;
}

/*
 * Start or restart an index scan
 */
void hnswrescan(IndexScanDesc scan, ScanKey keys, int nkeys, ScanKey orderbys, int norderbys)
{
	HnswScanOpaque so = (HnswScanOpaque)scan->opaque;

	/* 题目7：补全重置扫描状态的完整逻辑
	 * 核心提示：
	 * 1. 重置基础状态：
	 * 2. 重置内存上下文：MemoryContextReset(so->tmpCtx)
	 * 3. 拷贝扫描键/排序键：
	 *    - keys非空且scan->numberOfKeys>0时，memmove(scan->keyData, keys, ...)
	 *    - orderbys非空且scan->numberOfOrderBys>0时，memmove(scan->orderByData, orderbys, ...)
	 */
	/******** 填空开始 ********/
	so->first = true;
	/* v and discarded are allocated in tmpCtx */
	so->v.tids = NULL;
	so->discarded = NULL;
	so->tuples = 0;
	so->previousDistance = -get_float8_infinity();

	MemoryContextReset(so->tmpCtx);

	if (keys && scan->numberOfKeys > 0)
		memmove(scan->keyData, keys, scan->numberOfKeys * sizeof(ScanKeyData));

	if (orderbys && scan->numberOfOrderBys > 0)
		memmove(scan->orderByData, orderbys, scan->numberOfOrderBys * sizeof(ScanKeyData));
	/******** 填空结束 ********/
}

/*
 * Fetch the next tuple in the given scan
 */
bool hnswgettuple(IndexScanDesc scan, ScanDirection dir)
{
	HnswScanOpaque so = (HnswScanOpaque)scan->opaque;
	MemoryContext oldCtx = MemoryContextSwitchTo(so->tmpCtx);

	Assert(ScanDirectionIsForward(dir));

	if (so->first)
	{
		Datum value;

		pgstat_count_index_scan(scan->indexRelation);

		if (scan->orderByData == NULL)
			elog(ERROR, "cannot scan hnsw index without order");

		if (!IsMVCCSnapshot(scan->xs_snapshot))
			elog(ERROR, "non-MVCC snapshots are not supported with hnsw");

		value = GetScanValue(scan);

		LockPage(scan->indexRelation, HNSW_SCAN_LOCK, ShareLock);

		so->w = GetScanItems(scan, value);

		UnlockPage(scan->indexRelation, HNSW_SCAN_LOCK, ShareLock);

		so->first = false;

#if defined(HNSW_MEMORY)
		ShowMemoryUsage(so);
#endif
	}

	for (;;)
	{
		char *base = NULL;
		HnswSearchCandidate *sc;
		HnswElement element;
		ItemPointer heaptid;

		if (list_length(so->w) == 0)
		{
			/* 题目8：补全处理空候选集的完整分支逻辑
			 * 核心提示：
			 * 1. 迭代扫描关闭（hnsw_iterative_scan==HNSW_ITERATIVE_SCAN_OFF）：break
			 * 2. 索引为空（so->discarded==NULL）：break
			 * 3. 达到限制（元组超hnsw_max_scan_tuples 或 内存超so->maxMemory）：
			 *    - discarded非空则追加堆顶元素到so->w，否则break
			 * 4. 正常迭代：加锁→调用ResumeScanItems→解锁→（可选）打印内存
			 */
			/******** 填空开始 ********/
			if (hnsw_iterative_scan == HNSW_ITERATIVE_SCAN_OFF)
				break;

			/* Empty index */
			if (so->discarded == NULL)
				break;

			/* Reached max number of tuples or memory limit */
			if (so->tuples >= hnsw_max_scan_tuples || MemoryContextMemAllocated(so->tmpCtx, false) > so->maxMemory)
			{
				if (pairingheap_is_empty(so->discarded))
					break;

				/* Return remaining tuples */
				so->w = lappend(so->w, HnswGetSearchCandidate(w_node, pairingheap_remove_first(so->discarded)));
			}
			else
			{
				/*
				 * Locking ensures when neighbors are read, the elements they
				 * reference will not be deleted (and replaced) during the
				 * iteration.
				 *
				 * Elements loaded into memory on previous iterations may have
				 * been deleted (and replaced), so when reading neighbors, the
				 * element version must be checked.
				 */
				LockPage(scan->indexRelation, HNSW_SCAN_LOCK, ShareLock);

				so->w = ResumeScanItems(scan);

				UnlockPage(scan->indexRelation, HNSW_SCAN_LOCK, ShareLock);

#if defined(HNSW_MEMORY)
				ShowMemoryUsage(so);
#endif
			}
			/******** 填空结束 ********/

			if (list_length(so->w) == 0)
				break;
		}

		sc = llast(so->w);
		element = HnswPtrAccess(base, sc->element);

		/* 题目9：补全处理无有效堆TID的完整分支逻辑
		 * 核心提示：
		 * 1. 判断条件：element->heaptidsLength == 0
		 * 2. 操作步骤：
		 *    a. 删除列表最后元素
		 *    b. 迭代扫描未关闭时：pfree(element) + pfree(sc)
		 *    c. continue（处理下一个候选）
		 */
		/******** 填空开始 ********/
		if (element->heaptidsLength == 0)
		{
			so->w = list_delete_last(so->w);

			if (hnsw_iterative_scan != HNSW_ITERATIVE_SCAN_OFF)
			{
				pfree(element);
				pfree(sc);
			}
			continue;
		}
		/******** 填空结束 ********/

		heaptid = &element->heaptids[--element->heaptidsLength];

		/* 题目10：补全严格迭代扫描的距离校验完整逻辑
		 * 核心提示：
		 * 1. 仅严格模式（hnsw_iterative_scan==HNSW_ITERATIVE_SCAN_STRICT）需要校验
		 * 2. 若sc->distance < so->previousDistance：continue
		 * 3. 否则更新so->previousDistance = sc->distance
		 */
		/******** 填空开始 ********/
		if (hnsw_iterative_scan == HNSW_ITERATIVE_SCAN_STRICT)
		{
			if (sc->distance < so->previousDistance)
				continue;

			so->previousDistance = sc->distance;
		}
		/******** 填空结束 ********/

		MemoryContextSwitchTo(oldCtx);

		scan->xs_heaptid = *heaptid;
		scan->xs_recheck = false;
		scan->xs_recheckorderby = false;
		return true;
	}

	MemoryContextSwitchTo(oldCtx);
	return false;
}

/*
 * End a scan and release resources
 */
void hnswendscan(IndexScanDesc scan)
{
	/* 题目11：补全结束扫描释放资源的完整逻辑
	 * 核心提示：
	 * 1. 取出scan->opaque赋值给HnswScanOpaque so
	 * 2. 删除临时内存上下文
	 * 3. 释放so内存
	 * 4. 置空scan->opaque：scan->opaque = NULL
	 */
	/******** 填空开始 ********/
	HnswScanOpaque so = (HnswScanOpaque)scan->opaque;
	MemoryContextDelete(so->tmpCtx);
	pfree(so);
	scan->opaque = NULL;
	/******** 填空结束 ********/
}
