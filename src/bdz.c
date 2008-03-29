#include "bdz.h"
#include "cmph_structs.h"
#include "bdz_structs.h"
#include "hash.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
//#define DEBUG
#include "debug.h"
#define UNASSIGNED 3
#define NULL_EDGE 0xffffffff

//cmph_uint32 ngrafos = 0;
//cmph_uint32 ngrafos_aciclicos = 0;
// table used for looking up the number of assigned vertices  a 8-bit integer
const cmph_uint8 bdz_lookup_table[] =
{
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3, 2, 2, 2, 2, 1,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3, 2, 2, 2, 2, 1,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 3, 3, 3, 2,
3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3, 2, 2, 2, 2, 1,
3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3, 2, 2, 2, 2, 1,
3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3, 2, 2, 2, 2, 1,
3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3, 2, 2, 2, 2, 1,
2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 2, 1, 1, 1, 1, 0
};	

typedef struct 
{
	cmph_uint32 vertices[3];
	cmph_uint32 next_edges[3];
}bdz_edge_t;

typedef cmph_uint32 * bdz_queue_t;

static void bdz_alloc_queue(bdz_queue_t * queuep, cmph_uint32 nedges)
{
	(*queuep)=malloc(nedges*sizeof(cmph_uint32));
};
static void bdz_free_queue(bdz_queue_t * queue)
{
	free(*queue);
};

typedef struct 
{
	cmph_uint32 nedges;
	bdz_edge_t * edges;
	cmph_uint32 * first_edge;
	cmph_uint8 * vert_degree;	
}bdz_graph3_t;


static void bdz_alloc_graph3(bdz_graph3_t * graph3, cmph_uint32 nedges, cmph_uint32 nvertices)
{
	graph3->edges=malloc(nedges*sizeof(bdz_edge_t));
	graph3->first_edge=malloc(nvertices*sizeof(cmph_uint32));
	graph3->vert_degree=malloc(nvertices);	
};
static void bdz_init_graph3(bdz_graph3_t * graph3, cmph_uint32 nedges, cmph_uint32 nvertices)
{
	memset(graph3->first_edge,0xff,nvertices*sizeof(cmph_uint32));
	memset(graph3->vert_degree,0,nvertices);
	graph3->nedges=0;
};
static void bdz_free_graph3(bdz_graph3_t *graph3)
{
	free(graph3->edges);
	free(graph3->first_edge);
	free(graph3->vert_degree);
};

static void bdz_partial_free_graph3(bdz_graph3_t *graph3)
{
	free(graph3->first_edge);
	free(graph3->vert_degree);
	graph3->first_edge = NULL;
	graph3->vert_degree = NULL;
};

static void bdz_add_edge(bdz_graph3_t * graph3, cmph_uint32 v0, cmph_uint32 v1, cmph_uint32 v2)
{
	graph3->edges[graph3->nedges].vertices[0]=v0;
	graph3->edges[graph3->nedges].vertices[1]=v1;
	graph3->edges[graph3->nedges].vertices[2]=v2;
	graph3->edges[graph3->nedges].next_edges[0]=graph3->first_edge[v0];
	graph3->edges[graph3->nedges].next_edges[1]=graph3->first_edge[v1];
	graph3->edges[graph3->nedges].next_edges[2]=graph3->first_edge[v2];
	graph3->first_edge[v0]=graph3->first_edge[v1]=graph3->first_edge[v2]=graph3->nedges;
	graph3->vert_degree[v0]++;
	graph3->vert_degree[v1]++;
	graph3->vert_degree[v2]++;
	graph3->nedges++;
};

static void bdz_dump_graph(bdz_graph3_t* graph3, cmph_uint32 nedges, cmph_uint32 nvertices)
{
	int i;
	for(i=0;i<nedges;i++){
		printf("\nedge %d %d %d %d ",i,graph3->edges[i].vertices[0],
			graph3->edges[i].vertices[1],graph3->edges[i].vertices[2]);
		printf(" nexts %d %d %d",graph3->edges[i].next_edges[0],
				graph3->edges[i].next_edges[1],graph3->edges[i].next_edges[2]);
	};
	
	for(i=0;i<nvertices;i++){
		printf("\nfirst for vertice %d %d ",i,graph3->first_edge[i]);
	
	};
};

static void bdz_remove_edge(bdz_graph3_t * graph3, cmph_uint32 curr_edge)
{
	cmph_uint32 i,j=0,vert,edge1,edge2;
	for(i=0;i<3;i++){
		vert=graph3->edges[curr_edge].vertices[i];
		edge1=graph3->first_edge[vert];
		edge2=NULL_EDGE;
		while(edge1!=curr_edge&&edge1!=NULL_EDGE){
			edge2=edge1;
			if(graph3->edges[edge1].vertices[0]==vert){
				j=0;
			} else if(graph3->edges[edge1].vertices[1]==vert){
				j=1;
			} else 
				j=2;
			edge1=graph3->edges[edge1].next_edges[j];
		};
		if(edge1==NULL_EDGE){
			printf("\nerror remove edge %d dump graph",curr_edge);
			bdz_dump_graph(graph3,graph3->nedges,graph3->nedges+graph3->nedges/4);
			exit(-1);
		};
		
		if(edge2!=NULL_EDGE){
			graph3->edges[edge2].next_edges[j] = 
				graph3->edges[edge1].next_edges[i];
		} else 
			graph3->first_edge[vert]=
				graph3->edges[edge1].next_edges[i];
		graph3->vert_degree[vert]--;
	};
	
};

static int bdz_generate_queue(cmph_uint32 nedges, cmph_uint32 nvertices, bdz_queue_t queue, bdz_graph3_t* graph3)
{
	cmph_uint32 i,v0,v1,v2;
	cmph_uint32 queue_head=0,queue_tail=0;
	cmph_uint32 curr_edge;
	cmph_uint32 tmp_edge;
	cmph_uint8 * marked_edge =malloc((nedges >> 3) + 1);
	memset(marked_edge, 0, (nedges >> 3) + 1);

	for(i=0;i<nedges;i++){
		v0=graph3->edges[i].vertices[0];
		v1=graph3->edges[i].vertices[1];
		v2=graph3->edges[i].vertices[2];
		if(graph3->vert_degree[v0]==1 || 
				graph3->vert_degree[v1]==1 ||
				graph3->vert_degree[v2]==1){
			if(!GETBIT(marked_edge,i)) {
				queue[queue_head++]=i;
				SETBIT(marked_edge,i);
			}
		};
	};
	while(queue_tail!=queue_head){
		curr_edge=queue[queue_tail++];
		bdz_remove_edge(graph3,curr_edge);
		v0=graph3->edges[curr_edge].vertices[0];
		v1=graph3->edges[curr_edge].vertices[1];
		v2=graph3->edges[curr_edge].vertices[2];
		if(graph3->vert_degree[v0]==1 ) {
			tmp_edge=graph3->first_edge[v0];
			if(!GETBIT(marked_edge,tmp_edge)) {
				queue[queue_head++]=tmp_edge;
				SETBIT(marked_edge,tmp_edge);
			};
			
		};
		if(graph3->vert_degree[v1]==1) {
			tmp_edge=graph3->first_edge[v1];
			if(!GETBIT(marked_edge,tmp_edge)){
				queue[queue_head++]=tmp_edge;
				SETBIT(marked_edge,tmp_edge);
			};
			
		};
		if(graph3->vert_degree[v2]==1){
			tmp_edge=graph3->first_edge[v2];
			if(!GETBIT(marked_edge,tmp_edge)){
				queue[queue_head++]=tmp_edge;
				SETBIT(marked_edge,tmp_edge);
			};
		};
	};
	free(marked_edge);
	return queue_head-nedges;/* returns 0 if successful otherwies return negative number*/
};

static int bdz_mapping(cmph_config_t *mph, bdz_graph3_t* graph3, bdz_queue_t queue);
static void assigning(bdz_config_data_t *bdz, bdz_graph3_t* graph3, bdz_queue_t queue);
static void ranking(bdz_config_data_t *bdz);
static cmph_uint32 rank(cmph_uint8 b, cmph_uint32 * ranktable, cmph_uint8 * g, cmph_uint32 vertex);

bdz_config_data_t *bdz_config_new()
{
	bdz_config_data_t *bdz;
	bdz = (bdz_config_data_t *)malloc(sizeof(bdz_config_data_t));
	assert(bdz);
	memset(bdz, 0, sizeof(bdz_config_data_t));
	bdz->hashfunc = CMPH_HASH_JENKINS;
	bdz->g = NULL;
	bdz->hl = NULL;
	bdz->k = 0; //kth index in ranktable, $k = log_2(n=3r)/\varepsilon$
	bdz->b = 7; // number of bits of k
	bdz->ranktablesize = 0; //number of entries in ranktable, $n/k +1$
	bdz->ranktable = NULL; // rank table
	return bdz;
}

void bdz_config_destroy(cmph_config_t *mph)
{
	bdz_config_data_t *data = (bdz_config_data_t *)mph->data;
	DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

void bdz_config_set_b(cmph_config_t *mph, cmph_uint8 b)
{
	bdz_config_data_t *bdz = (bdz_config_data_t *)mph->data;
	if (b <= 2 || b > 10) b = 7; // validating restrictions over parameter b.
	bdz->b = b;
	DEBUGP("b: %u\n", b);

}

void bdz_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	bdz_config_data_t *bdz = (bdz_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 1) break; //bdz only uses one linear hash function
		bdz->hashfunc = *hashptr;	
		++i, ++hashptr;
	}
}

cmph_t *bdz_new(cmph_config_t *mph, float c)
{
	cmph_t *mphf = NULL;
	bdz_data_t *bdzf = NULL;
	cmph_uint32 iterations;
	bdz_queue_t edges;
	bdz_graph3_t graph3;
	bdz_config_data_t *bdz = (bdz_config_data_t *)mph->data;
	if (c == 0) c = 1.23; // validating restrictions over parameter c.
	DEBUGP("c: %f\n", c);
	bdz->m = mph->key_source->nkeys;	
	bdz->r = ceil((c * mph->key_source->nkeys)/3);	
	bdz->n = 3*bdz->r;

	bdz->k = (1 << bdz->b);
	DEBUGP("b: %u -- k: %u\n", bdz->b, bdz->k);
	
	bdz->ranktablesize = bdz->n/bdz->k + 2;
	DEBUGP("ranktablesize: %u\n", bdz->ranktablesize);

	
	bdz_alloc_graph3(&graph3, bdz->m, bdz->n);
	bdz_alloc_queue(&edges,bdz->m);
	DEBUGP("Created hypergraph\n");
	
	DEBUGP("m (edges): %u n (vertices): %u  r: %u c: %f \n", bdz->m, bdz->n, bdz->r, c);

	// Mapping step
	iterations = 1000;
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", bdz->m, bdz->n);
	}
	while(1)
	{
		int ok;
		DEBUGP("linear hash function \n");
		bdz->hl = hash_state_new(bdz->hashfunc, 15);

		ok = bdz_mapping(mph, &graph3, edges);
                //ok = 0;
		if (!ok)
		{
			--iterations;
			hash_state_destroy(bdz->hl);
			bdz->hl = NULL;
			DEBUGP("%u iterations remaining\n", iterations);
			if (mph->verbosity)
			{
				fprintf(stderr, "acyclic graph creation failure - %u iterations remaining\n", iterations);
			}
			if (iterations == 0) break;
		} 
		else break;
	}
	
	if (iterations == 0)
	{
		bdz_free_queue(&edges);
		bdz_free_graph3(&graph3);
		return NULL;
	}
	bdz_partial_free_graph3(&graph3);
	// Assigning step
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering assigning step for mph creation of %u keys with graph sized %u\n", bdz->m, bdz->n);
	}
	assigning(bdz, &graph3, edges);

	bdz_free_queue(&edges);
	bdz_free_graph3(&graph3);
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering ranking step for mph creation of %u keys with graph sized %u\n", bdz->m, bdz->n);
	}
	ranking(bdz);

	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	bdzf = (bdz_data_t *)malloc(sizeof(bdz_data_t));
	bdzf->g = bdz->g;
	bdz->g = NULL; //transfer memory ownership
	bdzf->hl = bdz->hl;
	bdz->hl = NULL; //transfer memory ownership
	bdzf->ranktable = bdz->ranktable;
	bdz->ranktable = NULL; //transfer memory ownership
	bdzf->ranktablesize = bdz->ranktablesize;
	bdzf->k = bdz->k;
	bdzf->b = bdz->b;
	bdzf->n = bdz->n;
	bdzf->m = bdz->m;
	bdzf->r = bdz->r;
	mphf->data = bdzf;
	mphf->size = bdz->m;

	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}

	return mphf;
}

		
static int bdz_mapping(cmph_config_t *mph, bdz_graph3_t* graph3, bdz_queue_t queue)
{
	cmph_uint32 e;
	int cycles = 0;
	cmph_uint32 hl[3];
	bdz_config_data_t *bdz = (bdz_config_data_t *)mph->data;
	bdz_init_graph3(graph3, bdz->m, bdz->n);
	mph->key_source->rewind(mph->key_source->data);
	for (e = 0; e < mph->key_source->nkeys; ++e)
	{
		cmph_uint32 h0, h1, h2;
		cmph_uint32 keylen;
		char *key = NULL;
		mph->key_source->read(mph->key_source->data, &key, &keylen);		
		hash_vector(bdz->hl, key, keylen,hl);
		h0 = hl[0] % bdz->r;
		h1 = hl[1] % bdz->r + bdz->r;
		h2 = hl[2] % bdz->r + (bdz->r << 1);
		mph->key_source->dispose(mph->key_source->data, key, keylen);
		bdz_add_edge(graph3,h0,h1,h2);
	}
	cycles = bdz_generate_queue(bdz->m, bdz->n, queue, graph3);	
	return (cycles == 0);
}

static void assigning(bdz_config_data_t *bdz, bdz_graph3_t* graph3, bdz_queue_t queue)
{
	cmph_uint32 i;
	cmph_uint32 nedges=graph3->nedges;
	cmph_uint32 curr_edge;
	cmph_uint32 v0,v1,v2;
	cmph_uint8 * marked_vertices =malloc((bdz->n >> 3) + 1);
	bdz->g = (cmph_uint8 *)calloc((bdz->n >> 2)+1, sizeof(cmph_uint8));	
	memset(marked_vertices, 0, (bdz->n >> 3) + 1);
	memset(bdz->g, 0xff, (bdz->n >> 2) + 1);

	for(i=nedges-1;i+1>=1;i--){
		curr_edge=queue[i];
		v0=graph3->edges[curr_edge].vertices[0];
		v1=graph3->edges[curr_edge].vertices[1];
		v2=graph3->edges[curr_edge].vertices[2];
		DEBUGP("B:%u %u %u -- %u %u %u\n", v0, v1, v2, GETVALUE(bdz->g, v0), GETVALUE(bdz->g, v1), GETVALUE(bdz->g, v2));
		if(!GETBIT(marked_vertices, v0)){
			if(!GETBIT(marked_vertices,v1))
			{
				SETVALUE1(bdz->g, v1, UNASSIGNED); 
				SETBIT(marked_vertices, v1);
			}
			if(!GETBIT(marked_vertices,v2))
			{
				SETVALUE1(bdz->g, v2, UNASSIGNED);		
				SETBIT(marked_vertices, v2);
			}
			SETVALUE1(bdz->g, v0, (6-(GETVALUE(bdz->g, v1) + GETVALUE(bdz->g,v2)))%3);
			SETBIT(marked_vertices, v0);
		} else if(!GETBIT(marked_vertices, v1)) {
			if(!GETBIT(marked_vertices, v2))
			{
				SETVALUE1(bdz->g, v2, UNASSIGNED);
				SETBIT(marked_vertices, v2);
			}
			SETVALUE1(bdz->g, v1, (7-(GETVALUE(bdz->g, v0)+GETVALUE(bdz->g, v2)))%3);
			SETBIT(marked_vertices, v1);
		}else {
			SETVALUE1(bdz->g, v2, (8-(GETVALUE(bdz->g,v0)+GETVALUE(bdz->g, v1)))%3);
			SETBIT(marked_vertices, v2);
		}		
		DEBUGP("A:%u %u %u -- %u %u %u\n", v0, v1, v2, GETVALUE(bdz->g, v0), GETVALUE(bdz->g, v1), GETVALUE(bdz->g, v2));
	};
	free(marked_vertices);
}


static void ranking(bdz_config_data_t *bdz)
{
	cmph_uint32 i, j, offset = 0, count = 0, size = (bdz->k >> 2), nbytes_total = (bdz->n >> 2)+1, nbytes;
	bdz->ranktable = (cmph_uint32 *)calloc(bdz->ranktablesize, sizeof(cmph_uint32));
	// ranktable computation
	bdz->ranktable[0] = 0;	
	i = 1;
	while(1)
	{
		nbytes = size < nbytes_total? size : nbytes_total;
		for(j = 0; j < nbytes; j++)
		{
			count += bdz_lookup_table[*(bdz->g + offset + j)];
		}
		if(i == bdz->ranktablesize) fprintf(stderr, "i:%u == bdz->ranktablesize:%u\n", i, bdz->ranktablesize);
		assert(i < bdz->ranktablesize); 
		bdz->ranktable[i] = count;
		offset += nbytes;
		if(size >= nbytes_total) break;
		nbytes_total -= size;
		i++;
	}
}


int bdz_dump(cmph_t *mphf, FILE *fd)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	bdz_data_t *data = (bdz_data_t *)mphf->data;
	__cmph_dump(mphf, fd);

	hash_state_dump(data->hl, &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	fwrite(&(data->n), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->m), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->r), sizeof(cmph_uint32), 1, fd);
	
	fwrite(data->g, sizeof(cmph_uint8)*((data->n >> 2) +1), 1, fd);

	fwrite(&(data->k), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->b), sizeof(cmph_uint8), 1, fd);
	fwrite(&(data->ranktablesize), sizeof(cmph_uint32), 1, fd);

	fwrite(data->ranktable, sizeof(cmph_uint32)*(data->ranktablesize), 1, fd);
	#ifdef DEBUG
	cmph_uint32 i;
	fprintf(stderr, "G: ");
	for (i = 0; i < data->n; ++i) fprintf(stderr, "%u ", GETVALUE(data->g, i));
	fprintf(stderr, "\n");
	#endif
	return 1;
}

void bdz_load(FILE *f, cmph_t *mphf)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	bdz_data_t *bdz = (bdz_data_t *)malloc(sizeof(bdz_data_t));

	DEBUGP("Loading bdz mphf\n");
	mphf->data = bdz;

	fread(&buflen, sizeof(cmph_uint32), 1, f);
	DEBUGP("Hash state has %u bytes\n", buflen);
	buf = (char *)malloc(buflen);
	fread(buf, buflen, 1, f);
	bdz->hl = hash_state_load(buf, buflen);
	free(buf);
	

	DEBUGP("Reading m and n\n");
	fread(&(bdz->n), sizeof(cmph_uint32), 1, f);	
	fread(&(bdz->m), sizeof(cmph_uint32), 1, f);	
	fread(&(bdz->r), sizeof(cmph_uint32), 1, f);	

	bdz->g = (cmph_uint8 *)calloc((bdz->n >> 2) + 1, sizeof(cmph_uint8));
	fread(bdz->g, ((bdz->n >> 2) + 1)*sizeof(cmph_uint8), 1, f);

	fread(&(bdz->k), sizeof(cmph_uint32), 1, f);
	fread(&(bdz->b), sizeof(cmph_uint8), 1, f);
	fread(&(bdz->ranktablesize), sizeof(cmph_uint32), 1, f);

	bdz->ranktable = (cmph_uint32 *)calloc(bdz->ranktablesize, sizeof(cmph_uint32));
	fread(bdz->ranktable, sizeof(cmph_uint32)*(bdz->ranktablesize), 1, f);

	#ifdef DEBUG
	cmph_uint32  i = 0;
	fprintf(stderr, "G: ");
	for (i = 0; i < bdz->n; ++i) fprintf(stderr, "%u ", GETVALUE(bdz->g,i));
	fprintf(stderr, "\n");
	#endif
	return;
}
		

cmph_uint32 bdz_search_ph(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	bdz_data_t *bdz = mphf->data;
	cmph_uint32 hl[3];
	hash_vector(bdz->hl, key, keylen, hl);
	cmph_uint32 vertex;
	hl[0] = hl[0] % bdz->r;
	hl[1] = hl[1] % bdz->r + bdz->r;
	hl[2] = hl[2] % bdz->r + (bdz->r << 1);
	vertex = hl[(GETVALUE(bdz->g, hl[0]) + GETVALUE(bdz->g, hl[1]) + GETVALUE(bdz->g, hl[2])) % 3];
	return vertex;
}

static inline cmph_uint32 rank(cmph_uint8 b, cmph_uint32 * ranktable, cmph_uint8 * g, cmph_uint32 vertex)
{
	register cmph_uint32 index = vertex >> b;
	register cmph_uint32 base_rank = ranktable[index];
	register cmph_uint32 beg_idx_v = index << b;
	register cmph_uint32 beg_idx_b = beg_idx_v >> 2;
	register cmph_uint32 end_idx_b = vertex >> 2;
	while(beg_idx_b < end_idx_b)
	{
		base_rank += bdz_lookup_table[*(g + beg_idx_b++)];
		
	}
	beg_idx_v = beg_idx_b << 2;
	while(beg_idx_v < vertex) 
	{
		if(GETVALUE(g, beg_idx_v) != UNASSIGNED) base_rank++;
		beg_idx_v++;
	}
	
	return base_rank;
}

cmph_uint32 bdz_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	register cmph_uint32 vertex;
	register bdz_data_t *bdz = mphf->data;
	cmph_uint32 hl[3];
	hash_vector(bdz->hl, key, keylen, hl);
	hl[0] = hl[0] % bdz->r;
	hl[1] = hl[1] % bdz->r + bdz->r;
	hl[2] = hl[2] % bdz->r + (bdz->r << 1);
	vertex = hl[(GETVALUE(bdz->g, hl[0]) + GETVALUE(bdz->g, hl[1]) + GETVALUE(bdz->g, hl[2])) % 3];
	return rank(bdz->b, bdz->ranktable, bdz->g, vertex);
}


void bdz_destroy(cmph_t *mphf)
{
	bdz_data_t *data = (bdz_data_t *)mphf->data;
	free(data->g);	
	hash_state_destroy(data->hl);
	free(data->ranktable);
	free(data);
	free(mphf);
}

/** cmph_uint32 bdz_search_fingerprint(cmph_t *mphf, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint);
 *  \brief Computes the mphf value and a fingerprint of 12 bytes (i.e., figerprint should be a prealocated area to fit three 4-byte integers). 
 *  \param mphf pointer to the resulting function
 *  \param key is the key to be hashed
 *  \param keylen is the key legth in bytes
 *  \return The mphf value
 * 
 * Computes the mphf value and a fingerprint of 12 bytes. The figerprint pointer should be 
 * a prealocated area to fit three 4-byte integers. You don't need to use all the 12 bytes
 * as fingerprint. According to the application, just few bits can be enough, once mphf does
 * not allow collisions for the keys previously known.
 */
cmph_uint32 bdz_search_fingerprint(cmph_t *mphf, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	register cmph_uint32 vertex;
	register bdz_data_t *bdz = mphf->data;
	cmph_uint32 hl[3];
	
	hash_vector(bdz->hl, key, keylen, hl);
	memcpy(fingerprint, hl, sizeof(hl));
	hl[0] = hl[0] % bdz->r;
	hl[1] = hl[1] % bdz->r + bdz->r;
	hl[2] = hl[2] % bdz->r + (bdz->r << 1);
	vertex = hl[(GETVALUE(bdz->g, hl[0]) + GETVALUE(bdz->g, hl[1]) + GETVALUE(bdz->g, hl[2])) % 3];
	return rank(bdz->b, bdz->ranktable, bdz->g, vertex);
}

/** \fn void bdz_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size() 
 */
void bdz_pack(cmph_t *mphf, void *packed_mphf)
{
	bdz_data_t *data = (bdz_data_t *)mphf->data;
	cmph_uint8 * ptr = packed_mphf;

	// packing hl type
	CMPH_HASH hl_type = hash_get_type(data->hl);
	*((cmph_uint32 *) ptr) = hl_type;
	ptr += sizeof(cmph_uint32);

	// packing hl
	hash_state_pack(data->hl, ptr);
	ptr += hash_state_packed_size(hl_type);

	// packing r
	*((cmph_uint32 *) ptr) = data->r;
	ptr += sizeof(data->r);

	// packing ranktablesize
	*((cmph_uint32 *) ptr) = data->ranktablesize;
	ptr += sizeof(data->ranktablesize);

	// packing ranktable
	memcpy(ptr, data->ranktable, sizeof(cmph_uint32)*(data->ranktablesize));
	ptr += sizeof(cmph_uint32)*(data->ranktablesize);

	// packing b
	*ptr++ = data->b;

	// packing g
	memcpy(ptr, data->g,  sizeof(cmph_uint8)*((data->n >> 2) +1));
}

/** \fn cmph_uint32 bdz_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 bdz_packed_size(cmph_t *mphf)
{
	bdz_data_t *data = (bdz_data_t *)mphf->data;

	CMPH_HASH hl_type = hash_get_type(data->hl); 

	return (sizeof(CMPH_ALGO) + hash_state_packed_size(hl_type) + 3*sizeof(cmph_uint32) + sizeof(cmph_uint32)*(data->ranktablesize) + sizeof(cmph_uint8) + sizeof(cmph_uint8)*((data->n >> 2) +1));
}

/** cmph_uint32 bdz_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search. 
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 bdz_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{
	
	register cmph_uint32 vertex;
	register CMPH_HASH hl_type  = *(cmph_uint32 *)packed_mphf;
	register cmph_uint8 *hl_ptr = (cmph_uint8 *)(packed_mphf + 4);

	register cmph_uint32 *ranktable = (cmph_uint32*)(hl_ptr + hash_state_packed_size(hl_type));
	
	register cmph_uint32 r = *ranktable++;
	register cmph_uint32 ranktablesize = *ranktable++;
	register cmph_uint8 * g = (cmph_uint8 *)(ranktable + ranktablesize);
	register cmph_uint8 b = *g++;

	cmph_uint32 hl[3];
	hash_vector_packed(hl_ptr, hl_type, key, keylen, hl);
	hl[0] = hl[0] % r;
	hl[1] = hl[1] % r + r;
	hl[2] = hl[2] % r + (r << 1);
	vertex = hl[(GETVALUE(g, hl[0]) + GETVALUE(g, hl[1]) + GETVALUE(g, hl[2])) % 3];
	return rank(b, ranktable, g, vertex);
}
