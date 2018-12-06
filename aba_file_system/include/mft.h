
#ifndef ABA_MFT_H_
#define ABA_MFT_H_

#include "types.h"
#include "declarations.h"

/*
 * Lee de disco la secuencia de bytes de un determinado cluster
 * cluster: cluster que se va a leer
 * data: 	buffer para almacenar la secuencia de bytes
 */
extern s32 ReadCluster(u64 cluster, u8 *data, FILE *fp);
/*
 * Escribe en disco una secuencia de bytes en un determinado cluster
 * cluster: cluster en que se va a escribir
 * data: 	buffer con la secuencia de bytes a escribir
 */
extern s32 WriteCluster(u64 cluster, u8 *data, FILE *fp);
/*
 * Lee un record de la mft
 * record_number: 	record que se va a leer
 * record: 			estructura para almacenar los datos del record
 */
extern s32 MftReadRecord(u64 record_number, mft_file_record *record, FILE *fp);
/*
 * Escribe un record de la mft
 * Procurar escribir el cluster y que los bytes restantes queden en 0
 * record_number: 	record que se va a escribir
 * record: 			estructura con los datos del record a escribir
 */
extern s32 MftWriteRecord(u64 record_number, mft_file_record *record, FILE *fp);
/*
 * Anade una entrada de directorio
 * dir: 	directorio al que se le anade la entrada
 * entry: 	entrada de directorio
 */
extern s32 AddDirEntry(mft_file_record *dir, directory_entry *entry, FILE *fp);
/*
 * Elimina una entrada de directorio
 * dir: 	directorio al que se le elimina la entrada
 * entry: 	entrada de directorio
 */
extern s32 RemoveDirEntry(mft_file_record *dir, u64 record_number, FILE *fp);
/*
 * Encuentra la entrada correspondiente al nombre del fichero especificado
 * Devuelve NULL si no encuentra la entrada
 * dir:			directorio en el que buscar la entrada
 * filename: 	nombre del fichero a buscar
 * fp:			puntero al fichero disco. Si se pasa NULL y el puntero es inicializado
 * 				dentro de la funcion, no se liberara la memoria correspondiente a dicho
 * 				puntero
 */
extern directory_entry* FindDirEntry(mft_file_record* dir, const char *filename, FILE *fp);
/*
 * Hace no residente el atributo data del record especificado
 */
extern s32 MakeNonResident(mft_file_record *record, FILE *fp);
/*
 * Busca en la Mft una entrada vacia
 * Devuelve el numero de la entrada
 */
extern u64 FindEmptyRecord(FILE *fp);
/*
 * Encuentra dado un path el record correspondiente y lo deja en actual
 * deja tambien en el paremetro parent el directorio padre del fichero
 */
extern s32 FindMftRecordOfPath(const char *path, mft_file_record *parent, mft_file_record *actual, FILE *fp);
/*
 * Copia el contenido del record src al record dest
 */
extern s32 CopyMftRecord(mft_file_record *dest, mft_file_record *src);
/*
 * Libera un record de la Mft
 */
extern s32 FreeMftRecord(mft_file_record *record, FILE *fp);
/*
 * Reserva un record de la Mft
 */
extern s32 ReserveMftRecord(u64 record, FILE *fp);
extern u64 FindCluster(u64 record, u64 requestCluster, FILE *fp);
extern s32 ChangeAllChild(u64 dataCluster, NON_RESIDENT_LEVEL nr_level, u32 *ndir_entry, u64 record, FILE *fp);

#endif /* ABA_MFT_H_ */
