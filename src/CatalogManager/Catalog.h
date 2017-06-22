#ifndef _Catalog_H_
#define _Catalog_H_
#include "../CONSTANT.h"
#include <string>

class TableMeta{
public:
	TableMeta(const std::string & table_name):table_name(table_name),attr_name_list(NULL),attr_type_list(NULL),
					attr_num(0),table_addr(0),primay_key_index(-1),primary_index_addr(0){}
	~TableMeta(){
		delete [] this->attr_name_list;
		delete [] this->attr_type_list;
	}
	const std::string table_name;
	uint32_t table_addr;
	int attr_num;
	std::string* attr_name_list;
	DBenum* attr_type_list;
	int primay_key_index;
	uint32_t primary_index_addr; // address of primary index
	std::string & GetAttrName(int index){return this->attr_name_list[index];}
	DBenum & GetAttrType(int index){return this->attr_type_list[index];}
};

class Catalog{
public:
	static Catalog & Instance(){
		static Catalog theCatalog;
		return theCatalog;
	}
	~Catalog(){}

	void CreateDatabase(const std::string & db_name);
	void UseDatabase(const std::string & db_name);

	void CreateTable(const std::string & table_name, std::string* attr_name_list, DBenum* attr_type_list, int attr_num, int key_index);
	TableMeta* GetTableMeta(const std::string table_name);

	void CreateIndex(const std::string & table_name, int8_t secondary_key_index);
	uint32_t Catalog::GetIndex(const std::string & table_name, int8_t secondary_key_index);

private:
	Catalog();
	Catalog(const Catalog&);
	Catalog & operator=(const Catalog&);

	uint32_t database_block_addr;
	uint32_t user_block_addr;

	uint32_t table_data_addr;
	uint32_t table_index_addr;

	uint32_t index_data_addr;
	uint32_t index_index_addr;

	bool database_selected;
};

#endif