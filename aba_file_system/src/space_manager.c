
#define FUSE_USE_VERSION 25
#define _FILE_OFFSET_BITS 64

#include "space_manager.h"
#include "bitmap.h"
#include "mem_alloc.h"
#include "mft.h"

u64 FindEmptyClusters(u64 *len, FILE *fp){
	u64 cluster=0;
	u64 count = *len;
	u32 pos = logb2(count);
	struct BuddyNode *node = (struct BuddyNode*)xmalloc(sizeof(struct BuddyNode));
	if(BuddyList[pos]->count > 0 && count == powi(2, pos)){
		RemoveBuddyNode(pos, node);
		cluster = node->cluster;
		free(node);
		return cluster;
	}
	else{
		s32 i;
		for(i=pos+1; i<10; i++){
			if(BuddyList[i]->count > 0){
				RemoveBuddyNode(i, node);
				AddBuddyNode(node->cluster + count, powi(2,i) - count);
				cluster = node->cluster;
				free(node);
				return cluster;
			}
		}
		for(i=pos-1; i>=0; i--){
			if(BuddyList[i]->count > 0){
				RemoveBuddyNode(i, node);
				*len = node->count;
				cluster = node->cluster;
				free(node);
				return cluster;
			}
		}
	}
	
	*len = 0;
	return 0;
}

s32 FreeCluster(u64 cluster, FILE *fp){
	u64 clusterBmp = 1 + cluster/(BOOT_SECTOR->cluster_size*8);
	u64 offset = cluster%(BOOT_SECTOR->cluster_size*8);
	u8* bitmap = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	s32 errorCode=0;
	if((errorCode = ReadCluster(clusterBmp, bitmap, fp)) != 0)
		return errorCode;
	bit_set(bitmap, offset, 0);
	if((errorCode = WriteCluster(clusterBmp, bitmap, fp)) != 0)
		return errorCode;
	free(bitmap);
	if(cluster > BOOT_SECTOR->mft_zone_clusters + BOOT_SECTOR->logical_mft_cluster)
		errorCode = AddBuddyNode(cluster, 1);
	return errorCode;
}

s32 ReserveCluster(u64 cluster, FILE *fp){
	u64 clusterBmp = 1 + cluster/(BOOT_SECTOR->cluster_size*8);
	u64 offset = cluster%(BOOT_SECTOR->cluster_size*8);
	u8* bitmap = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	s32 errorCode=0;
	if((errorCode = ReadCluster(clusterBmp, bitmap, fp)) != 0)
		return errorCode;
	bit_set(bitmap, offset, 1);
	if((errorCode = WriteCluster(clusterBmp, bitmap, fp)) != 0)
		return errorCode;
	free(bitmap);
	return errorCode;
}

s32 CopyCluster(u64 clusterDest, u64 clusterSrc, FILE *fp){
	u8 *dataSrc = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	s32 errorCode=0;
	if((errorCode = ReadCluster(clusterSrc, dataSrc, fp)) != 0)
		return errorCode;
	if((errorCode = WriteCluster(clusterDest, dataSrc, fp)) != 0)
		return errorCode;
	return errorCode;
}

s32 CleanCluster(u64 cluster, FILE *fp){
	s32 res = 0;
	u8 *data = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, data, fp)) != 0){
		return res;
	}
	memset(data, 0, BOOT_SECTOR->cluster_size);
	if((res = WriteCluster(cluster, data, fp)) != 0){
		return res;
	}
	free(data);
	return res;
}

s32 LoadBuddySystem(FILE *fp){
	u32 i;
	for(i=0; i<10; i++){
		BuddyList[i] = (struct BuddyNode*)xcalloc(1, sizeof(struct BuddyNode));
	}
	u32 bitmapcount = BOOT_SECTOR->logical_mft_cluster-1;
	u64 bitmapsize = bitmapcount*BOOT_SECTOR->cluster_size;
	u32 errorCode=0;
	u8 *bitmap = (u8*)xcalloc(1, bitmapsize);
	
	for(i=0; i<bitmapcount; i++){
		if((errorCode = ReadCluster(i + 1, bitmap + BOOT_SECTOR->cluster_size*i, fp)) != 0){
			return errorCode;
		}
	}
	// place the clusters after the mft_zone
	u64 bitmapBitsCount = BOOT_SECTOR->disk_size/BOOT_SECTOR->cluster_size;
	u32 startBit = BOOT_SECTOR->logical_mft_cluster + BOOT_SECTOR->mft_zone_clusters + 2;
	u32 count=0;
	u32 j=startBit;
	while(j < bitmapBitsCount){
		if(bit_get(bitmap, j) == 0){
			count++;
		}
		else{
			AddBuddyNode(j-count, count);
			count=0;
		}
		j++;
	}
	if(count > 0)
		AddBuddyNode(j-count, count);
	free(bitmap);
	return 0;
}

s32 AddBuddyNode(u64 cluster, u64 count){
	u64 currentCluster = cluster;
	s64 currentCount = count;
	s32 k=9;
	while(currentCount > 0){
		u32 countBlock = powi(2, k);
		if(currentCount >= countBlock){
			struct BuddyNode *node = (struct BuddyNode*)xcalloc(1, sizeof(struct BuddyNode));
			node->cluster = currentCluster;
			node->count = countBlock;
			node->next=NULL;

			struct BuddyNode *tmp = BuddyList[k];
			if(tmp->count == 0){
				memcpy(BuddyList[k], node, sizeof(struct BuddyNode));
				BuddyList[k]->count = 0;
			}
			else{
				struct BuddyNode *prev = NULL;
				while(tmp){
					if(tmp->cluster == cluster + count){
						if(!prev){
							if(tmp->next){
								u32 c = BuddyList[k]->count - 1;
								BuddyList[k] = tmp->next;
								BuddyList[k]->count = c;
								return AddBuddyNode(cluster, countBlock + currentCount);
							}
							BuddyList[k]->cluster=0;
							BuddyList[k]->count=0;
							return AddBuddyNode(cluster, countBlock + currentCount);
						}
						BuddyList[k]->count = BuddyList[k]->count - 1;
						prev->next = tmp->next;
						return AddBuddyNode(cluster, countBlock + currentCount);
					}
					if(tmp->cluster == cluster - count){
						if(!prev){
							u64 cc = BuddyList[k]->cluster;
							if(tmp->next){
								u32 c = BuddyList[k]->count - 1;
								BuddyList[k] = tmp->next;
								BuddyList[k]->count = c;
								return AddBuddyNode(cc, countBlock + currentCount);
							}
							BuddyList[k]->cluster=0;
							BuddyList[k]->count=0;
							return AddBuddyNode(cc, countBlock + currentCount);
						}
						BuddyList[k]->count = BuddyList[k]->count - 1;
						prev->next = tmp->next;
						return AddBuddyNode(tmp->cluster, countBlock + currentCount);
					}
					prev = tmp;
					tmp = tmp->next;
				}
				prev->next = node;
			}
			BuddyList[k]->count++;
			currentCount -= countBlock;
			currentCluster += countBlock;
		}
		else
			k--;
	}
	return 0;
}

s32 RemoveBuddyNode(u32 pos, struct BuddyNode *node){
	u64 queueCount=0;
	memcpy(node, BuddyList[pos], sizeof(struct BuddyNode));
	queueCount = BuddyList[pos]->count;
	BuddyList[pos] = BuddyList[pos]->next;
	
	if(queueCount == 1){
		BuddyList[pos] = (struct BuddyNode*)xcalloc(1, sizeof(struct BuddyNode));
	}
	BuddyList[pos]->count = queueCount-1;
	return 0;
}

s32 PrintBuddySystem(FILE *fp){
	u32 i;
	for(i=0; i<10; i++){
		fprintf(fp, "BuddyList[%i]\t", i);
		struct BuddyNode *tmp = BuddyList[i];
		while(tmp){
			fprintf(fp, "%u, %u\t", tmp->count, (u32)tmp->cluster);
			tmp = tmp->next;
		}
		fprintf(fp, "\n");
		fflush(fp);
	}
	return 0;
}

s32 PrintBitmap(FILE *fp, FILE *disk){
	u32 bitmapcount = BOOT_SECTOR->logical_mft_cluster-1;
	u64 bitmapsize = bitmapcount*BOOT_SECTOR->cluster_size;
	u32 errorCode=0;
	u8 *bitmap = (u8*)xcalloc(1, bitmapsize);
	
	u32 i;
	for(i=0; i<bitmapcount; i++){
		if((errorCode = ReadCluster(i + 1, bitmap + BOOT_SECTOR->cluster_size*i, disk)) != 0){
			return errorCode;
		}
	}
	u64 bitmapBitsCount = BOOT_SECTOR->disk_size/BOOT_SECTOR->cluster_size;
	i=0;
	while(i < bitmapBitsCount){
		u32 value = bit_get(bitmap, i);
		fprintf(fp, "%i", value);
		i++;
		if(i % 95 == 0)
			fprintf(fp, "\n");
		fflush(fp);
	}
	free(bitmap);
	return 0;
}
