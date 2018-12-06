
#ifndef FREE_SPACE_MANAGER_H_
#define FREE_SPACE_MANAGER_H_

#include "types.h"
#include "error.h"

/*
 * Estructura que indica un conjunto de clusters consecutivos, donde
 * el atributo count indica el tamano de ese conjunto
 * el atributo cluster donde empieza el conjunto
 * el atributo next es un puntero al proximo en la lista de los conjuntos
 * de clusters consecutivos del mismo tamano
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
 * Copia el contenido de clusterSrc a clusterDest
 */
extern s32 CopyCluster(u64 clusterDest, u64 clusterSrc, FILE *fp);
/*
 * Marca como no usado el cluster especificado en el bitmap
 */
extern s32 FreeCluster(u64 cluster, FILE *fp);
/*
 * Marca como ocupado en el bitmap el cluster especificado
 */
extern s32 ReserveCluster(u64 cluster, FILE *fp);
/*
 * Limpia un cluster
 */
extern s32 CleanCluster(u64 cluster, FILE *fp);
/*
 * Carga las listas del buddy-system para administrar el espacio libre
 */
extern s32 LoadBuddySystem();
/*
 * Annade un nuevo nodo a las listas del buddy-system
 */
extern s32 AddBuddyNode(u64 cluster, u64 count);
/*
 * Extrae un nodo de las listas de buddy-system
 * El nodo extraido es depositado en el parametro node
 */
extern s32 RemoveBuddyNode(u32 count, struct BuddyNode *node);

s32 PrintBuddySystem(FILE *fp);
s32 PrintBitmap(FILE *fp, FILE *disk);
#endif /* FREE_SPACE_MANAGER_H_ */
