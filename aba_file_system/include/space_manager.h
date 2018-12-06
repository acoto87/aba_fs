
#ifndef FREE_SPACE_MANAGER_H_
#define FREE_SPACE_MANAGER_H_

#include "types.h"
#include "error.h"

/*
 * Represent a set of consecutive clusters, where
 * count indicate the size of the set
 * cluster is where the set begin
 * next is a pointer to the next consecutive set of clusters of the same size
 */
struct BuddyNode{
	u32 count;
	u64 cluster;
	struct BuddyNode *next;
}*BuddyList[10];

static __inline__ u32 logb2(u32 x){
	u32 count=0;
	u32 z = x/2;
	while(z != 0){
		count++;
		z /= 2;
	}
	return count;
}

static __inline__ u64 powi(u64 x, u64 y){
	if(x == 1) return 1;
	if(y == 0) return 1;
	return x*powi(x, y-1);
}

/*
 * Busca en el disco un bloque de clusters de tamano 'len' y devuelve el primero
 * dicho bloque y deja en 'len' el tamano del bloque que se encontro.
 * Si no encuentra ningun bloque de tamano 'len' devuelve el bloque
 * de longitud mayor que se menor que 'len'
 * len: longitud requerida
 */
extern u64 FindEmptyClusters(u64 *len, FILE *fp);
/*
 * Copy the contents of clusterSrc to clusterDest
 */
extern s32 CopyCluster(u64 clusterDest, u64 clusterSrc, FILE *fp);
/*
 * Mark a cluster as not used
 */
extern s32 FreeCluster(u64 cluster, FILE *fp);
/*
 * Mark a cluster as used
 */
extern s32 ReserveCluster(u64 cluster, FILE *fp);
/*
 * Clean a cluster
 */
extern s32 CleanCluster(u64 cluster, FILE *fp);
/*
 * Load the buddy-system lists to manage the empty space
 */
extern s32 LoadBuddySystem();
/*
 * Add a new node to the buddy-system lists
 */
extern s32 AddBuddyNode(u64 cluster, u64 count);
/*
 * Extract a node of the buddy-system lists
 * The node is returned in the node parameter
 */
extern s32 RemoveBuddyNode(u32 count, struct BuddyNode *node);

/*
 * Debug utilities
 */
s32 PrintBuddySystem(FILE *fp);
s32 PrintBitmap(FILE *fp, FILE *disk);

#endif /* FREE_SPACE_MANAGER_H_ */
