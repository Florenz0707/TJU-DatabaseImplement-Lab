#include "postgres.h"

#include "access/amapi.h"
#include "catalog/index.h"
#include "access/reloptions.h"
#include "access/hnsw.h"       // 对应 ../src/include/access/hnsw.h
#include "commands/vacuum.h"
#include "utils/guc.h"

int			hnsw_lock_tranche_id;

// 提前声明所有函数（避免未定义错误）
bool hnswinsert(Relation index, Datum *values, bool *isnull,
                       ItemPointer heap_t_ctid, Relation heap,
                       IndexUniqueCheck checkUnique,
                       bool indexUnchanged,
                       IndexInfo *indexInfo);
IndexScanDesc hnswbeginscan(Relation index, int nkeys, int norderbys);
void hnswrescan(IndexScanDesc scan, ScanKey keys, int nkeys,
                       ScanKey orderbys, int norderbys);
void hnswendscan(IndexScanDesc scan);
static bytea *hnswoptions(Datum reloptions, bool validate);

// ------------------------------
// 实际函数实现
// ------------------------------

IndexScanDesc
hnswbeginscan(Relation index, int nkeys, int norderbys)
{
    ereport(ERROR, (errmsg("hnsw access method not implemented yet: beginscan")));
    return NULL;
}

void
hnswrescan(IndexScanDesc scan, ScanKey keys, int nkeys,
           ScanKey orderbys, int norderbys)
{
    ereport(ERROR, (errmsg("hnsw access method not implemented yet: rescan")));
}

void
hnswendscan(IndexScanDesc scan)
{
    /* 暂无操作 */
}

static bytea *
hnswoptions(Datum reloptions, bool validate)
{
    static const relopt_parse_elt tab[] = {
        {"m", RELOPT_TYPE_INT, offsetof(HnswOptions, m)},
        {"ef_construction", RELOPT_TYPE_INT, offsetof(HnswOptions, efConstruction)}
    };

    HnswOptions *opts;

    opts = (HnswOptions *) build_reloptions(reloptions, validate, RELOPT_KIND_BTREE,
                                            sizeof(HnswOptions),
                                            tab, lengthof(tab));

    /* 设置默认值 */
    if (opts->m <= 0)
        opts->m = 16;
    if (opts->efConstruction <= 0)
        opts->efConstruction = 64;

    return (bytea *) opts;
}

// ------------------------------
// 核心 Handler 函数（适配 PostgreSQL 版本）
// ------------------------------
// Handler 函数标记（内核级注册）
PG_FUNCTION_INFO_V1(hnswhandler);
Datum
hnswhandler(PG_FUNCTION_ARGS)
{
    IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

    // 索引特性配置（适配版本：amoptsprocnum 替代 amoptsproc）
    amroutine->amstrategies = 0;
    amroutine->amsupport = 3;
    amroutine->amoptsprocnum = 0;
    
    amroutine->amcanorder = false;
    amroutine->amcanorderbyop = true;
    amroutine->amcanbackward = false;
    amroutine->amcanunique = false;
    amroutine->amcanmulticol = false;
    amroutine->amoptionalkey = true;
    amroutine->amsearcharray = false;
    amroutine->amsearchnulls = false;
    amroutine->amstorage = false;
    amroutine->amclusterable = false;
    amroutine->ampredlocks = false;
    amroutine->amcanparallel = false;
    amroutine->amcaninclude = false;
    amroutine->amusemaintenanceworkmem = true;
    amroutine->amparallelvacuumoptions = VACUUM_OPTION_PARALLEL_BULKDEL;
    amroutine->amkeytype = InvalidOid;

    // 绑定索引生命周期函数
    amroutine->ambuild = hnswbuild;
    amroutine->ambuildempty = hnswbuildempty;
    amroutine->aminsert = hnswinsert;
    amroutine->ambulkdelete = NULL;
    amroutine->amvacuumcleanup = NULL;
    amroutine->amcanreturn = NULL;
    amroutine->amcostestimate = NULL;
    amroutine->amoptions = hnswoptions;
    amroutine->amproperty = NULL;
    amroutine->ambuildphasename = NULL;
    amroutine->amvalidate = NULL;
    amroutine->ambeginscan = hnswbeginscan;
    amroutine->amrescan = hnswrescan;
    amroutine->amgettuple = NULL;
    amroutine->amgetbitmap = NULL;
    amroutine->amendscan = hnswendscan;
    amroutine->ammarkpos = NULL;
    amroutine->amrestrpos = NULL;
    amroutine->amestimateparallelscan = NULL;
    amroutine->aminitparallelscan = NULL;
    amroutine->amproperty = NULL;
    amroutine->amparallelrescan = NULL;

    PG_RETURN_POINTER(amroutine);
}

void
HnswInitLockTranche(void)
{
	int		   *tranche_ids;
	bool		found;

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
	tranche_ids = ShmemInitStruct("hnsw LWLock ids",
								  sizeof(int) * 1,
								  &found);
	if (!found)
	{
		tranche_ids[0] = LWLockNewTrancheId();
	}
	hnsw_lock_tranche_id = tranche_ids[0];
	LWLockRelease(AddinShmemInitLock);


	/* Per-backend registration of the tranche ID */
	LWLockRegisterTranche(hnsw_lock_tranche_id, "HnswBuild");

}