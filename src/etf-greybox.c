
#include "runtime.h"
#include "etf-util.h"
#include "etf-greybox.h"
#include <tables.h>
#include "dm/dm.h"

static void etf_popt(poptContext con,
 		enum poptCallbackReason reason,
                            const struct poptOption * opt,
                             const char * arg, void * data){
	(void)con;(void)opt;(void)arg;(void)data;
	switch(reason){
	case POPT_CALLBACK_REASON_PRE:
		break;
	case POPT_CALLBACK_REASON_POST:
		GBregisterLoader("etf",ETFloadGreyboxModel);
		Warning(info,"ETF language module initialized");
		return;
	case POPT_CALLBACK_REASON_OPTION:
		break;
	}
	Fatal(1,error,"unexpected call to etf_popt");
}
struct poptOption etf_options[]= {
	{ NULL, 0 , POPT_ARG_CALLBACK|POPT_CBFLAG_POST|POPT_CBFLAG_SKIPOPTION , etf_popt , 0 , NULL , NULL },
	POPT_TABLEEND
};

typedef struct grey_box_context {
	treedbs_t label_db;
	int edge_labels;
	treedbs_t* trans_db;
	matrix_table_t* trans_table;
	treedbs_t* label_key;
	int** label_data;
} *gb_context_t;

static int etf_short(model_t self,int group,int*src,TransitionCB cb,void*user_context){
    gb_context_t ctx=(gb_context_t)GBgetContext(self);
    uint32_t src_no=(uint32_t)TreeFold(ctx->trans_db[group],src);
    int dst[dm_ones_in_row(GBgetDMInfo(self), group)];
    //Warning(info,"group %d, state %d",group,src_no);
    matrix_table_t mt=ctx->trans_table[group];
    if ((src_no)>=MTclusterCount(mt)) return 0;
    int K=MTclusterSize(mt,src_no);
    for(int i=0;i<K;i++){
        uint32_t row[3];
        MTclusterGetRow(mt,src_no,i,row);
        TreeUnfold(ctx->trans_db[group],(int)row[1],dst);
        switch(ctx->edge_labels){
            case 0:
                cb(user_context,NULL,dst);
                break;
            case 1: {
                int lbl=(int)row[2];
                cb(user_context,&lbl,dst);
                break;
            }
            default: {
                int lbl[ctx->edge_labels];
                TreeUnfold(ctx->label_db,(int)row[2],lbl);
                cb(user_context,lbl,dst);
                break;
            }
        }
    }
    return K;
}

static int etf_state_short(model_t self,int label,int *state){
    gb_context_t ctx=(gb_context_t)GBgetContext(self);
    return ctx->label_data[label][TreeFold(ctx->label_key[label],state)];
}

void ETFloadGreyboxModel(model_t model,const char*name){
	gb_context_t ctx=(gb_context_t)RTmalloc(sizeof(struct grey_box_context));
	GBsetContext(model,ctx);
	etf_model_t etf=etf_parse_file(name);
	lts_type_t ltstype=etf_type(etf);
	int state_length=lts_type_get_state_length(ltstype);
	ctx->edge_labels=lts_type_get_edge_label_count(ltstype);
	if (ctx->edge_labels>1) {
		ctx->label_db=TreeDBScreate(ctx->edge_labels);
	} else {
		ctx->label_db=NULL;
	}
	GBsetLTStype(model,ltstype);
	matrix_t* p_dm_info = (matrix_t*)RTmalloc(sizeof(matrix_t));
	dm_create(p_dm_info, etf_trans_section_count(etf), state_length);
	ctx->trans_db=(treedbs_t*)RTmalloc(dm_nrows(p_dm_info)*sizeof(treedbs_t));
        ctx->trans_table=(matrix_table_t*)RTmalloc(dm_nrows(p_dm_info)*sizeof(matrix_table_t));
	for(int i=0; i < dm_nrows(p_dm_info); i++) {
		Warning(info,"parsing table %d",i);
		etf_rel_t trans=etf_trans_section(etf,i);
		int used[state_length];
		int src[state_length];
		int dst[state_length];
		int lbl[ctx->edge_labels];
		int proj[state_length];
		ETFrelIterate(trans);
		if (!ETFrelNext(trans,src,dst,lbl)){
			Fatal(1,error,"unexpected empty transition section");
		}
		int len=0;
		for(int j=0;j<state_length;j++){
			if (src[j]) {
				proj[len]=j;
				Warning(debug,"pi[%d]=%d",len,proj[len]);
				len++;
				dm_set(p_dm_info, i, j);
			    used[j]=1;
			} else {
			    used[j]=0;
			}
		}
		Warning(info,"length is %d",len);
		ctx->trans_db[i]=TreeDBScreate(len);
                ctx->trans_table[i]=MTcreate(3);
		int src_short[len];
		int dst_short[len];
                uint32_t row[3];
		do {
			for(int k=0;k<state_length;k++) {
				if(used[k]?(src[k]==0):(src[k]!=0)){
					Fatal(1,error,"inconsistent section in src vector");
				}
			}
			for(int k=0;k<len;k++) src_short[k]=src[proj[k]]-1;
			for(int k=0;k<state_length;k++) {
				if(used[k]?(dst[k]==0):(dst[k]!=0)){
					Fatal(1,error,"inconsistent section in dst vector");
				}
			}
			for(int k=0;k<len;k++) dst_short[k]=dst[proj[k]]-1;
			row[0]=(uint32_t)TreeFold(ctx->trans_db[i],src_short);
			switch(ctx->edge_labels){
				case 0:
					row[2]=0;
					break;
				case 1:
					row[2]=(uint32_t)lbl[0];
					break;
				default:
                                        row[2]=(uint32_t)TreeFold(ctx->label_db,lbl);
					break;
			}
			row[1]=(int32_t)TreeFold(ctx->trans_db[i],dst_short);
                        MTaddRow(ctx->trans_table[i],row);
		} while(ETFrelNext(trans,src,dst,lbl));
		Warning(info,"table %d has %d states and %d transitions",
                        i,TreeCount(ctx->trans_db[i]),ETFrelCount(trans));
		ETFrelDestroy(&trans);
        MTclusterBuild(ctx->trans_table[i],0,TreeCount(ctx->trans_db[i]));
	}
	GBsetDMInfo(model, p_dm_info);
	GBsetNextStateShort(model,etf_short);

	matrix_t *p_sl_info = RTmalloc(sizeof *p_sl_info);
	dm_create(p_sl_info, etf_map_section_count(etf), state_length);
	ctx->label_key=(treedbs_t*)RTmalloc(dm_nrows(p_sl_info)*sizeof(treedbs_t));
	ctx->label_data=(int**)RTmalloc(dm_nrows(p_sl_info)*sizeof(int*));
	for(int i=0;i<dm_nrows(p_sl_info);i++){
		Warning(info,"parsing map %d",i);
		etf_map_t map=etf_get_map(etf,i);
		int used[state_length];
		int state[state_length];
		int value;
		ETFmapIterate(map);
		if (!ETFmapNext(map,state,&value)){
			Fatal(1,error,"Unexpected empty map");
		}
		int len=0;
		for(int j=0;j<state_length;j++){
			if (state[j]) {
				used[len]=j;
				len++;
				dm_set(p_sl_info, i, j);
			}
		}
		int*proj=(int*)RTmalloc(len*sizeof(int));
		for(int j=0;j<len;j++) proj[j]=used[j];
		for(int j=0;j<state_length;j++) used[j]=state[j];
		treedbs_t key_db=TreeDBScreate(len);
		int *data=(int*)RTmalloc(ETFmapCount(map)*sizeof(int));
		int key[len];
		do {
			for(int k=0;k<state_length;k++) {
				if(used[k]?(state[k]==0):(state[k]!=0)){
					Fatal(1,error,"inconsistent map section");
				}
			}
			for(int k=0;k<len;k++) key[k]=state[proj[k]]-1;
			data[TreeFold(key_db,key)]=value;
		} while(ETFmapNext(map,state,&value));
		ctx->label_key[i]=key_db;
		ctx->label_data[i]=data;
	}
    GBsetStateLabelInfo(model, p_sl_info);
	GBsetStateLabelShort(model,etf_state_short);

	int type_count=lts_type_get_type_count(ltstype);
	for(int i=0;i<type_count;i++){
		Warning(info,"Setting values for type %d (%s)",i,lts_type_get_type(ltstype,i));
		int count=etf_get_value_count(etf,i);
		for(int j=0;j<count;j++){
			if (j!=GBchunkPut(model,i,etf_get_value(etf,i,j))){
				Fatal(1,error,"etf-greybox does not support remapping of values");
			}
		}
	}

	int state[state_length];
	etf_get_initial(etf,state);
	GBsetInitialState(model,state);
}
