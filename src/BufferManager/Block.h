#ifndef _BLOCK_H_
#define _BLOCK_H_

#define BLOCK_SIZE 4096
#define BLOCK_HEAD_SIZE 10
#include "../CONSTANT.h"
#include "../EXCEPTION.h"
#include <cstdint>
#include <cstring>

// only class Block has real data, 
// other successor only provide addtional method
class Block{
public:
	Block(){
		this->block_data = new uint8_t[BLOCK_SIZE];
	}
	Block(uint8_t* buf):block_data(buf){}
	virtual ~Block(){
		delete [] this->block_data;
	}
	virtual void Init(uint32_t index, DBenum block_type); 
	uint8_t & BlockType(){
		return *((uint8_t*)&(this->block_data[0]));
	}
	uint8_t & ReservedBytes(){
		return *((uint8_t*)&(this->block_data[1]));
	}
	uint32_t & BlockIndex(){
		return *((uint32_t*)&(this->block_data[2]));
	}
	uint32_t & NextBlockIndex(){
		return *((uint32_t*)&(this->block_data[6]));
	}

	uint8_t* block_data;
	bool is_dirty; //dirty tag
};

class SchemaBlock:public Block{
public:
	SchemaBlock():Block(){
		this->EmptyPtr() = BLOCK_HEAD_SIZE;
		this->EmptyBlockAddr() = 0;
		this->UserMetaAddr() = 0;
		this->DBMetaAddr() = 0;		
	}
	SchemaBlock(uint8_t* buf):Block(buf){}
	virtual ~SchemaBlock() {}
	uint16_t & EmptyPtr(){
		return *((uint16_t*)&(this->block_data[BLOCK_HEAD_SIZE]));
	}
	uint32_t & EmptyBlockAddr(){
		return *((uint32_t*)&(this->block_data[BLOCK_HEAD_SIZE + 2]));
	}
	uint32_t & DBMetaAddr(){
		return *((uint32_t*)&(this->block_data[BLOCK_HEAD_SIZE + 6]));
	}
	uint32_t & UserMetaAddr(){
		return *((uint32_t*)&(this->block_data[BLOCK_HEAD_SIZE + 10]));
	}
	uint32_t & IndexMetaAddr(){
		return *((uint32_t*)&(this->block_data[BLOCK_HEAD_SIZE + 14]));
	}
	uint16_t EmptyLen(){
		return BLOCK_SIZE - this->EmptyPtr();
	}
};

class TableBlock:public Block{
public:
	TableBlock():Block(){
		this->RecordNum() = 0;
		this->StackPtr() = BLOCK_SIZE - 1;		
	}
	TableBlock(uint8_t* buf):Block(buf){}
	virtual ~TableBlock(){}
	uint16_t & RecordNum(){
		return *((uint16_t*)&(this->block_data[BLOCK_HEAD_SIZE]));
	}
	uint16_t & StackPtr(){
		return *((uint16_t*)&(this->block_data[BLOCK_HEAD_SIZE + 2]));
	}
	short EmptySize(){
		return this->StackPtr()+1 - DATA_BEG - this->RecordNum() * TABLE_RECORD_SIZE;
	}
	uint8_t* GetTableInfoPtr(unsigned short row){
		return (uint8_t*)&(this->block_data[DATA_BEG + (row)*TABLE_RECORD_SIZE]);	
	}
	void InsertTable(const char* table_name, uint32_t table_addr, uint32_t index_addr, uint8_t attr_num, uint8_t key_index);
	void InsertAttr(const char* attr_name, DBenum attr_type);
	unsigned short FindRecordIndex(const char* table_name);
	void DropTable(const char* table_name);
	void GetTableMeta(const char* table_name, uint32_t &, uint32_t &, uint8_t&, uint16_t&, uint8_t&);
	void GetAttrMeta(char* attr_name, DBenum & attr_type, uint16_t attr_addr);
	static const size_t DATA_BEG = BLOCK_HEAD_SIZE + 4;
	static const size_t TABLE_RECORD_SIZE =  48;
	static const size_t ATTR_RECORD_SIZE = 36;
};

class RecordBlock:public Block{
public:
	RecordBlock():Block(){
		this->RecordNum() = 0;
		this->size = NULL;
		this->type = NULL;
	}
	RecordBlock(uint8_t* buf):Block(buf){
		this->size = NULL;
		this->type = NULL;
	}
	virtual ~RecordBlock(){
		delete [] this->size;
		delete [] this->type;
	}
	void Format(DBenum* attr_type, uint16_t attr_num, unsigned short key);
	uint16_t & RecordNum(){
		return *(uint16_t*)&(this->block_data[BLOCK_HEAD_SIZE]);
	}
	bool CheckEmptySpace(){
		if(BLOCK_SIZE - BLOCK_HEAD_SIZE - 2 - this->RecordNum() * this->tuple_size < this->tuple_size){
			return false;
		}
		else{
			return true;
		}
	}
	uint8_t* GetDataPtr(unsigned short row, unsigned short colomn);
	unsigned short FindTupleIndex(const void* key_data);
	int InsertTuple(const void** data_list);
	void RemoveTuple(const uint8_t* key_data);
	int Compare(uint8_t* data_1_ptr, uint8_t* data_2_ptr, unsigned short data_index);
	unsigned short tuple_size;
private:
	static const size_t DATA_BEG = BLOCK_HEAD_SIZE + 2;
	unsigned short* size;
	DBenum* type;
	unsigned short key_index;
	unsigned short attr_num;
};

#endif