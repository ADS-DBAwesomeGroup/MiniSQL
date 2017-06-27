
#include "APIStructures.h"
#include "APIFunctions.h"
#include "IO.h"
#include "../CatalogManager/Catalog.h"
#include "../IndexManager/IndexManager.h"
#include "../Type/ConstChar.h"
#include <string>
#include <sstream>

//may add to record manager
RecordBlock* insertTupleSafe(const void** tuple, TableMeta* tableMeta,  RecordBlock* dstBlock,BufferManager* bufferManager) {
	if (!dstBlock->CheckEmptySpace()) {
		RecordBlock* newBlock = dynamic_cast<RecordBlock*>(bufferManager->CreateBlock(DB_RECORD_BLOCK));
		dstBlock->NextBlockIndex() = newBlock->BlockIndex();
		bufferManager->ReleaseBlock((Block*&)(dstBlock));
		dstBlock = newBlock;
		dstBlock->Format(tableMeta->attr_type_list, tableMeta->attr_num, tableMeta->key_index);
	}
	dstBlock->InsertTuple(tuple);
	return dstBlock;
}
//may add to record manager
//compare template function
template <class T>
inline bool compare(const void* a, const void* b, const std::string &operation) {
	if (operation == ">") {
		return *(T*)a > *(T*)b;
	}
	else if (operation == ">=") {
		return *(T*)a >= *(T*)b;
	}
	else if (operation == "<") {
		return *(T*)a < *(T*)b;
	}
	else if (operation == "<=") {
		return *(T*)a <= *(T*)b;
	}
	else if (operation == "=") {
		return *(T*)a == *(T*)b;
	}
	else if (operation == "<>"||operation=="!=") {
		return *(T*)a != *(T*)b;
	}
	else {
		return false;
	}
}

//may add to record manager
//check if a tuple is valid
//cmpVec is sorted
inline bool checkTuple(RecordBlock* block, int line, TableMeta* tableMeta,const ComparisonVector& sortedCmpVec) {
	const ComparisonVector& cmpVec = sortedCmpVec;
	std::string cmpOperator;
	for (int i = 0,j = 0;cmpVec.begin()+i < cmpVec.end(); i++) {
		std::string cmpOperator = cmpVec[i].Operation;
		for (; j < tableMeta->attr_num; j++) {
			bool result=true;
			if (cmpVec[i].Comparand1.Content == tableMeta->GetAttrName(j)) {
				stringstream ss(cmpVec[i].Comparand2.Content);
				DBenum type = tableMeta->attr_type_list[j];
				switch (type) {
					case DB_TYPE_INT:
						int integer;
						ss >> integer;
						result = compare<int>(block->GetDataPtr(line, j),&integer,cmpOperator);
						break;
					case DB_TYPE_FLOAT:
						float num;
						ss >> num;
						result = compare<float>(block->GetDataPtr(line, j), &num, cmpOperator);
						break;
					default:
						if (type - DB_TYPE_CHAR < 16) {
							ConstChar<16> str;
							ss >> str;
							result = compare<ConstChar<16>>(block->GetDataPtr(line, j), &str, cmpOperator);
						}
						else if (type - DB_TYPE_CHAR < 33) {
							ConstChar<33> str;
							ss >> str;
							result = compare<ConstChar<33>>(block->GetDataPtr(line, j), &str, cmpOperator);
						}
						else if (type - DB_TYPE_CHAR < 64) {
							ConstChar<64> str;
							ss >> str;
							result = compare<ConstChar<64>>(block->GetDataPtr(line, j), &str, cmpOperator);
						}
						else if (type - DB_TYPE_CHAR < 128) {
							ConstChar<128> str;
							ss >> str;
							result = compare<ConstChar<128>>(block->GetDataPtr(line, j), &str, cmpOperator);
						}
						else {
							ConstChar<256> str;
							ss >> str;
							result = compare<ConstChar<256>>(block->GetDataPtr(line, j), &str, cmpOperator);
						}
						break;
				}
				if (!result) return result;
			}
		}	
	}
	return true;
}

// call Flush() after cout.
// Do not call cin, call GetString() / GetInt() / GetFloat() if necessary

void BeginQuery()
{

}

//Assumption: the first operand is always attribute
//the second is always a constant
//cmpVec is sorted
void ExeSelect(const TableAliasMap& tableAlias, const string& sourceTableName,
	const string& resultTableName, const ComparisonVector& cmpVec)
{

	//variables init
	Catalog* catalog = &Catalog::Instance();
	BufferManager* bufferManager = &BufferManager::Instance();
	IndexManager* indexManager;
	int indexPos;
	std::string operation = "";
	for (auto i = tableAlias.begin(); i != tableAlias.end(); i++) {
		cout << i->first << " " << i->second << endl;
	}
	//make sure table name is valid
	std::string tableName;
	try {
		tableName = tableAlias.at(sourceTableName);
	}
	catch (exception& e) {
		throw(e);
	}
	TableMeta* tableMeta = catalog->GetTableMeta(tableName);
	const void** tuple = (const void**)(new void*[tableMeta->attr_num]);
	RecordBlock* srcBlock;
	vector<Comparison> indexCmp;

	//create new temp table
	catalog->CreateTable(resultTableName, tableMeta->attr_name_list, tableMeta->attr_type_list, tableMeta->attr_num, tableMeta->key_index);
	RecordBlock* dstBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(catalog->GetTableMeta(resultTableName)->table_addr));
	dstBlock->is_dirty = true;
	dstBlock->Format(tableMeta->attr_type_list, tableMeta->attr_num, tableMeta->key_index);

	//try to find a proper index
	Block* indexRoot = nullptr;
	DBenum indexType;
	std::string indexContent;
	bool isPrimary = false; //if it is a primary index
	std::string primaryKeyName = (tableMeta->key_index >= 0) ?
		tableMeta->GetAttrName(tableMeta->key_index) : "";
	for (auto i = cmpVec.begin();i < cmpVec.end();i++) {
		//check if the attribute is a primary index
		if (primaryKeyName == (*i).Comparand1.Content) {
			indexRoot = bufferManager->GetBlock(tableMeta->primary_index_addr);
			indexType = tableMeta->GetAttrType(tableMeta->key_index);
			indexContent = (*i).Comparand1.Content;
			indexPos = i - cmpVec.begin();
			isPrimary = true;
			operation = (*i).Operation;
			break;
		}
		//check if the attribute is a secondary index
		else {
			for (int j = 0;j < tableMeta->attr_num;j++) {
				uint32_t index = catalog->GetIndex(tableName,j);
				if (index) {
					indexRoot = bufferManager->GetBlock(catalog->GetIndex(tableName, index));
					indexType = tableMeta->GetAttrType(j);
					operation = (*i).Operation;
					indexContent = (*i).Comparand1.Content;
					indexPos = i - cmpVec.begin();
				}
			}
		}
	}
	
	//primary index found
	if (isPrimary) {
		indexCmp.push_back(cmpVec[indexPos]);
		indexCmp[0].Operation = ">";
	}
	if (isPrimary&&(operation==">"||operation=="="||operation==">=")) {
		uint32_t ptr; // the result block's index
		stringstream ss(indexContent);
		SearchResult* pos;
		indexManager = getIndexManager(indexType);
		switch (indexType) {
		case DB_TYPE_INT:
			int integer;
			ss >> integer;
			pos = indexManager->searchEntry(indexRoot, BPTree, &integer);
			break;
		case DB_TYPE_FLOAT:
			float num;
			ss >> num;
			pos = indexManager->searchEntry(indexRoot, BPTree, &num);
			break;
		default:
			pos = indexManager->searchEntry(indexRoot, BPTree, (void*)indexContent.c_str());
			break;
		}
		//
		//TODO: check if a result is found or not
		//
		ptr = *(pos->ptrs + pos->index);
		srcBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(ptr));
		delete pos;
	}
	//no primary index found
	else {
		srcBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(tableMeta->table_addr));
	}
	while (true) {
		srcBlock->Format(tableMeta->attr_type_list, tableMeta->attr_num, tableMeta->key_index);
		for (int i = 0; i < srcBlock->RecordNum(); i++) {
			//if the current key is already larger than the searched primary index
			if (isPrimary&&(operation == "<" || operation == "=" || operation == "<=")) {
				if (checkTuple(srcBlock, i, tableMeta, indexCmp)) {
					bufferManager->ReleaseBlock((Block* &)srcBlock);
					break;
				}
			}
			//if the tuple fit the comparisonVector
			if (checkTuple(srcBlock, i, tableMeta, cmpVec)) {
				for (int j = 0;j < tableMeta->attr_num;j++) {
					tuple[j] = srcBlock->GetDataPtr(i, j);
				}
				//safe insert
				dstBlock = insertTupleSafe(tuple, tableMeta, dstBlock, bufferManager);
			}
		}
		uint32_t next = srcBlock->NextBlockIndex();
		if (next == 0) {
			break;
		}
		bufferManager->ReleaseBlock((Block* &)srcBlock);
		srcBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(next));
	}
	//delete allocated memory
	bufferManager->ReleaseBlock((Block* &)srcBlock);
	bufferManager->ReleaseBlock((Block* &)dstBlock);
	if(indexRoot) bufferManager->ReleaseBlock((Block* &)indexRoot);
	delete indexManager;
	delete tableMeta;
	delete[] tuple;
	ExeOutputTable(tableAlias, "T2");
}


//attrVec is sorted
void ExeProject(const TableAliasMap& tableAlias, const string& sourceTableName,
	const string& resultTableName, const AttrNameAliasVector& attrVec)
{
	Catalog* catalog = &Catalog::Instance();
	BufferManager* bufferManager = &BufferManager::Instance();
	TableMeta* tableMeta = catalog->GetTableMeta(tableAlias.at(sourceTableName));
	Block* block = bufferManager->GetBlock(tableMeta->table_addr);
	std::vector<int> attrIndexVec;
	//get attr index
	for (int i = 0, j = 0;i < tableMeta->attr_num&&j < (int)attrVec.size();i++) {
		if (tableMeta->GetAttrName(i) == attrVec[j].AttrName) {
			attrIndexVec.push_back(i);
			j++;
		}
	}
	//create new temp table
	std::string *newNameList;
	DBenum *newTypeList;
	newNameList = new std::string[attrIndexVec.size()];
	newTypeList = new DBenum[attrIndexVec.size()];
	for (int i = 0;i < (int)attrIndexVec.size();i++) {
		newNameList[i] = tableMeta->GetAttrName(attrIndexVec[i]);
		newTypeList[i] = tableMeta->GetAttrType(attrIndexVec[i]);
	}
	int keyIndex = 0;
	catalog->CreateTable(resultTableName, newNameList, newTypeList, tableMeta->attr_num, keyIndex);
	RecordBlock* dstBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(catalog->GetTableMeta(resultTableName)->table_addr));
	dstBlock->is_dirty = true;
	dstBlock->Format(newTypeList, attrIndexVec.size(), keyIndex);
	TableMeta* newTableMeta = catalog->GetTableMeta(resultTableName);

	//insert data into new table
	RecordBlock* srcBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(tableMeta->table_addr));
	const void** tuple = (const void**)(new void*[attrIndexVec.size()]);
	while (true) {
		srcBlock->Format(tableMeta->attr_type_list, tableMeta->attr_num, tableMeta->key_index);
		for (int i = 0; i < srcBlock->RecordNum(); i++) {
			for (int j = 0;j < (int)attrIndexVec.size();j++) {
				tuple[j] = srcBlock->GetDataPtr(i, attrIndexVec[j]);
			}
			//safe insert
			dstBlock = insertTupleSafe(tuple, newTableMeta, dstBlock,bufferManager);
		}
		uint32_t next = srcBlock->NextBlockIndex();
		if (next == 0) {
			break;
		}
		bufferManager->ReleaseBlock((Block* &)srcBlock);
		srcBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(next));
	}
	//delete allocated memory
	bufferManager->ReleaseBlock((Block* &)srcBlock);
	bufferManager->ReleaseBlock((Block* &)dstBlock);
	delete tableMeta;
	delete[] tuple;
	delete[] newNameList;
	delete[] newTypeList;
}

//attr is sorted
void ExeNaturalJoin(const TableAliasMap& tableAlias, const string& sourceTableName1,
	const string& sourceTableName2, const string& resultTableName)
{
	//variables init
	Catalog* catalog = &Catalog::Instance();
	BufferManager* bufferManager = &BufferManager::Instance();
	std::string cmpOperator = "=";
	//make sure table name is valid
	std::string tableName1,tableName2;
	try {
		tableName1 = tableAlias.at(sourceTableName1);
		tableName2 = tableAlias.at(sourceTableName2);
	}
	catch (exception& e) {
		throw(e);
	}
	TableMeta* tableMeta1 = catalog->GetTableMeta(tableName1), *tableMeta2 = catalog->GetTableMeta(tableName2);
	//const void** tuple = (const void**)(new void*[tableMeta->attr_num]);
	RecordBlock* srcBlock1,*srcBlock2;
	std::vector<Comparison> indexCmp;
	std::vector<int> commonAttrIndex1, commonAttrIndex2;

	//get common attrs
	for (int i = 0, j = 0;i < tableMeta1->attr_num&&j < tableMeta2->attr_num;) {
		if (tableMeta1->GetAttrName(i) > tableMeta2->GetAttrName(j)) j++;
		else if (tableMeta1->GetAttrName(i) < tableMeta2->GetAttrName(j)) i++;
		else {
			commonAttrIndex1.push_back(i);
			commonAttrIndex2.push_back(j);
		}
	}

	//create new temp table
	std::string *newNameList;
	DBenum *newTypeList;
	int newListSize = tableMeta1->attr_num + tableMeta2->attr_num - commonAttrIndex1.size();
	const void** tuple = (const void**)(new void*[newListSize]);
	newNameList = new std::string[newListSize];
	newTypeList = new DBenum[newListSize];
	for (int i = 0, j = 0, k = 0;i < tableMeta1->attr_num&&j < tableMeta2->attr_num;k++) {
		if (tableMeta1->GetAttrName(i) > tableMeta2->GetAttrName(j)) {
			j++;
			newNameList[k] = tableMeta2->GetAttrName(j);
			newTypeList[k] = tableMeta2->GetAttrType(j);
		}
		else {
			i++;
			newNameList[k] = tableMeta2->GetAttrName(i);
			newTypeList[k] = tableMeta2->GetAttrType(i);
		}
		if (tableMeta1->GetAttrName(i) == tableMeta2->GetAttrName(j)) j++;
	}
	int keyIndex = 0;
	catalog->CreateTable(resultTableName, newNameList, newTypeList, newListSize, keyIndex);
	RecordBlock* dstBlock = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(catalog->GetTableMeta(resultTableName)->table_addr));
	dstBlock->is_dirty = true;
	dstBlock->Format(newTypeList, newListSize, keyIndex);
	TableMeta* newTableMeta = catalog->GetTableMeta(resultTableName);

	//insert data into new table
	srcBlock1 = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(tableMeta1->table_addr));
	srcBlock2 = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(tableMeta2->table_addr));
	//first loop:srcBlock1
	while (true) {
		srcBlock1->Format(tableMeta1->attr_type_list, tableMeta1->attr_num, tableMeta1->key_index);
		for (int i = 0; i < srcBlock1->RecordNum(); i++) {
			//second loop:srcBlock2
			while (true) {
				srcBlock2->Format(tableMeta2->attr_type_list, tableMeta2->attr_num, tableMeta2->key_index);
				for (int j = 0; j < srcBlock1->RecordNum(); j++) {
					bool result = true; //compare result
					for (int k = 0;k < (int)commonAttrIndex1.size();k++) {
						DBenum type = tableMeta1->attr_type_list[commonAttrIndex1[k]];
						switch (type) {
						case DB_TYPE_INT:
							result = compare<int>(srcBlock1->GetDataPtr(i, commonAttrIndex1[k]), 
								srcBlock1->GetDataPtr(j, commonAttrIndex2[k]), cmpOperator);
							break;
						case DB_TYPE_FLOAT:
							result = compare<float>(srcBlock1->GetDataPtr(i, commonAttrIndex1[k]),
								srcBlock1->GetDataPtr(j, commonAttrIndex2[k]), cmpOperator);
							break;
						default:
							if (type - DB_TYPE_CHAR < 16) {
								result = compare<ConstChar<16>>(srcBlock1->GetDataPtr(i, commonAttrIndex1[k]),
									srcBlock1->GetDataPtr(j, commonAttrIndex2[k]), cmpOperator);
							}
							else if (type - DB_TYPE_CHAR < 33) {
								result = compare<ConstChar<33>>(srcBlock1->GetDataPtr(i, commonAttrIndex1[k]),
									srcBlock1->GetDataPtr(j, commonAttrIndex2[k]), cmpOperator);
							}
							else if (type - DB_TYPE_CHAR < 64) {
								result = compare<ConstChar<64>>(srcBlock1->GetDataPtr(i, commonAttrIndex1[k]),
									srcBlock1->GetDataPtr(j, commonAttrIndex2[k]), cmpOperator);
							}
							else if (type - DB_TYPE_CHAR < 128) {
								result = compare<ConstChar<128>>(srcBlock1->GetDataPtr(i, commonAttrIndex1[k]),
									srcBlock1->GetDataPtr(j, commonAttrIndex2[k]), cmpOperator);
							}
							else {
								result = compare<ConstChar<256>>(srcBlock1->GetDataPtr(i, commonAttrIndex1[k]),
									srcBlock1->GetDataPtr(j, commonAttrIndex2[k]), cmpOperator);
							}
							break;
						}
						if (!result) break;
					}
					//join two tuples
					if (result) {
						for (int ii = 0,jj=0;ii < tableMeta1->attr_num;ii++) {
							for (;jj < newTableMeta->attr_num;jj++) {
								if (tableMeta1->GetAttrName(ii) == newTableMeta->GetAttrName(jj)) {
									tuple[jj] = srcBlock1->GetDataPtr(i, ii);
									break;
								}
							}
						}
						for (int ii = 0, jj = 0;ii < tableMeta2->attr_num;ii++) {
							for (;jj < newTableMeta->attr_num;jj++) {
								if (tableMeta2->GetAttrName(ii) == newTableMeta->GetAttrName(jj)) {
									tuple[jj] = srcBlock2->GetDataPtr(j, ii);
									break;
								}
							}
						}
					}
					dstBlock = insertTupleSafe(tuple, newTableMeta, dstBlock, bufferManager);
				}
				uint32_t next = srcBlock2->NextBlockIndex();
				if (next == 0) {
					bufferManager->ReleaseBlock((Block* &)srcBlock2);
					break;
				}
				bufferManager->ReleaseBlock((Block* &)srcBlock2);
				srcBlock2 = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(next));
			}
		}
		uint32_t next = srcBlock1->NextBlockIndex();
		if (next == 0) {
			bufferManager->ReleaseBlock((Block* &)srcBlock1);
			break;
		}
		bufferManager->ReleaseBlock((Block* &)srcBlock1);
		srcBlock1 = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(next));
	}
	//delete allocated memory
	bufferManager->ReleaseBlock((Block* &)dstBlock);
	delete tableMeta1;
	delete tableMeta2;
	delete newTableMeta;
	delete[] tuple;
	delete[] newNameList;
	delete[] newTypeList;
}

void ExeCartesian(const TableAliasMap& tableAlias, const string& sourceTableName1,
	const string& sourceTableName2, const string& resultTableName)
{

}

void ExeOutputTable(const TableAliasMap& tableAlias, const string& sourceTableName)
{
	// Print "end_result" in the last line to stop


	// if table on disk
	Catalog* catalog = &Catalog::Instance();
	BufferManager* bufferManager = &BufferManager::Instance();
	TableMeta* tableMeta = catalog->GetTableMeta(sourceTableName);
	unsigned short record_key = tableMeta->key_index < 0 ? 0 : tableMeta->key_index;	

	RecordBlock* result_block_ptr = dynamic_cast<RecordBlock*>(bufferManager->GetBlock(tableMeta->table_addr));
	result_block_ptr->Format(tableMeta->attr_type_list, tableMeta->attr_num, record_key);
	while(true){
		for(unsigned int i = 0; i < result_block_ptr->RecordNum(); i++){
			for(int j = 0; j < tableMeta->attr_num; j++){
				switch(tableMeta->attr_type_list[j]){
					case DB_TYPE_INT: cout <<  *(int*)result_block_ptr->GetDataPtr(i, j);  break;
					case DB_TYPE_FLOAT: cout <<  *(float*)result_block_ptr->GetDataPtr(i, j);  break;
					default: cout <<  (char*)result_block_ptr->GetDataPtr(i, j);  break;
				}
			}
		}
		uint32_t next = result_block_ptr->NextBlockIndex();
		if(next == 0){ 
			bufferManager->ReleaseBlock((Block* &)result_block_ptr);
			break;
		}
		bufferManager->ReleaseBlock((Block* &)result_block_ptr);
		result_block_ptr =  dynamic_cast<RecordBlock*>(bufferManager->GetBlock(next));
	}

	// if table is a temperary table not on disk
		//TODO
}

void EndQuery()
{

}

int ptr_compare(const void* p1, const void* p2, DBenum type){
	switch(type){
		case DB_TYPE_INT:
			return *(int*) p1 - *(int*)p2;
			break;
		case DB_TYPE_FLOAT:
			return (int)(*(float*)p1 - *(float*)p2);
			break;
		default:
			return strcmp((char*)p1, (char*)p2);
			break;
	}
}

void ExeInsert(const std::string& tableName, InsertValueVector& values)
{
	Catalog* catalog = &Catalog::Instance();
	BufferManager* buffer_manager = &BufferManager::Instance();
	TableMeta* table_meta = catalog->GetTableMeta(tableName);
	vector<int> temp_int_vector;
	vector<float> temp_float_vector;
	vector<string> temp_string_vector;
	int temp_int;
	float temp_float;
	string temp_string;
	if(table_meta->attr_num != values.size()){
		cout << "Attributes Number Unmatch" << endl;
		return;
	}
	bool error = false;
	const void** data_list = new const void*[table_meta->attr_num];
	for(int i = 0; !error && i < table_meta->attr_num; i++){
		stringstream ss(values[i]);
		ss.exceptions(std::ios::failbit);
		switch(table_meta->attr_type_list[i]){
			case DB_TYPE_INT:
				ss >> temp_int;
				temp_int_vector.push_back(temp_int);
				data_list[i] = &temp_int_vector[temp_int_vector.size()-1];
				break;
			case DB_TYPE_FLOAT:
				ss >> temp_float;
				temp_float_vector.push_back(temp_float);
				data_list[i] = &temp_float_vector[temp_int_vector.size()-1];
				break;
			default:
				if(table_meta->attr_type_list[i] - DB_TYPE_CHAR < (int)values[i].length()){
					error = true;
					break;
				}
				ss >> temp_string;
				temp_string_vector.push_back(temp_string);
				data_list[i] = temp_string_vector[temp_string_vector.size()-1].c_str();
				break;
		}
	}
	if(error){
		cout << "Attributes Types Not Satisfied" << endl;
		return;
	}
	else{
		uint32_t record_block_addr;
		bool _is_distinct = true;
		/* do finding update index */
		IndexManager* index_manager = getIndexManager(table_meta->attr_type_list[table_meta->key_index]);
		Block* index_root = buffer_manager->GetBlock(table_meta->primary_index_addr);
		SearchResult* result_ptr = index_manager->searchEntry(index_root, BPTree, (void*)data_list[table_meta->key_index]);
		DBenum key_type = table_meta->attr_type_list[table_meta->key_index];
		void* B_plus_tree_key_ptr = NULL;
		if(result_ptr){
			if(ptr_compare(result_ptr->data, data_list[table_meta->key_index], key_type) == 0){
				if(table_meta->is_primary_key){
					cout << "Duplicated Primary Key" << endl;
					buffer_manager->ReleaseBlock(index_root);
					delete result_ptr;
					return;					
				}
				else{
					record_block_addr = *(result_ptr->ptrs + result_ptr->index);
				}
			}
			else{
				if(result_ptr->index == 0){
					record_block_addr = *(result_ptr->ptrs + result_ptr->index);
				}
				else{
					result_ptr->index--;
					record_block_addr = *(result_ptr->ptrs + result_ptr->index - 1);		
				}
			}
		}
		else{
			record_block_addr = table_meta->table_addr;
		}	

		// do real insert
		RecordBlock* record_block_ptr = dynamic_cast<RecordBlock*>(buffer_manager->GetBlock(record_block_addr));
		record_block_ptr->Format(table_meta->attr_type_list, table_meta->attr_num, table_meta->key_index);

		if(table_meta->is_primary_key){
			int i = record_block_ptr->FindTupleIndex(data_list[table_meta->key_index]);
			if(i >= 0 && ptr_compare(data_list[table_meta->key_index], record_block_ptr->GetDataPtr(i, table_meta->key_index), key_type) == 0){
				cout << "Duplicated Primary key" << endl;
				buffer_manager->ReleaseBlock(index_root);
				buffer_manager->ReleaseBlock((Block* &)record_block_ptr);
				delete result_ptr;
				return;
			}
			record_block_ptr->InsertTupleByIndex(data_list, i>=0?i:0);
		}
		else{
			record_block_ptr->InsertTuple(data_list);
		}

		// update index
		if(!result_ptr){
			index_manager->insertEntry(index_root, BPTree, record_block_ptr->GetDataPtr(0, table_meta->key_index), record_block_addr);
		}
		else{
			index_manager->removeEntry(index_root, BPTree, result_ptr);
			index_manager->insertEntry(index_root, BPTree, record_block_ptr->GetDataPtr(0, table_meta->key_index), record_block_addr);
		}
		//check space, split if needed
		if(!record_block_ptr->CheckEmptySpace()){
			RecordBlock* new_block_ptr = catalog->SplitRecordBlock(record_block_ptr, table_meta->attr_type_list, 
													table_meta->attr_num, table_meta->key_index);
			index_manager->insertEntry(index_root, BPTree, new_block_ptr->GetDataPtr(0, table_meta->key_index), record_block_addr);
			buffer_manager->ReleaseBlock((Block* &)new_block_ptr);
		}
		// update index root
		if(index_root->BlockIndex() != table_meta->primary_index_addr){
			catalog->UpdateTablePrimaryIndex(tableName, index_root->BlockIndex());
		}
		buffer_manager->ReleaseBlock((Block* &)record_block_ptr);
		buffer_manager->ReleaseBlock(index_root);
		delete result_ptr;
	}
	delete data_list;
	delete table_meta;
	cout << "1 Row Affected" << endl;
}

void ExeUpdate(const std::string& tableName, const std::string& attrName, 
	const std::string& value, const ComparisonVector& cmpVec)
{
	// Print result information in one line
}

//
void ExeDelete(const std::string& tableName, const ComparisonVector& cmpVec)
{
	//Catalog* catalog = &Catalog::Instance();
	//BufferManager* buffer_manager = &BufferManager::Instance();
	//TableMeta* table_meta = catalog->GetTableMeta(tableName);

	//bool use_primary_index = false;
	//for(int i = 0; i < cmpVec.size(); i++){
	//	if(attr_name_list[i] == table_meta->attr_name_list[table_meta->key_index]){
	//		use_primary_index = true;
	//	}
	//}

	//if(use_primary_index){
	//	IndexManager* index_manager = getIndexManager(table_meta->attr_type_list[table_meta->key_index]);
	//	Block* index_root = buffer_manager.GetBlock(table_meta->primary_index_addr);
	//	
	//}
	//else{

	//}

}

void ExeDropIndex(const std::string& tableName, const std::string& indexName)
{
	Catalog* catalog = &Catalog::Instance();
	try{
		catalog->DropIndex(indexName);
	}
	catch(const IndexNotFound){
		cout << "Index `" << indexName << "` Not Found" << endl;
		return;
	}
	cout << "Drop Index Named `" << indexName << "` Successfully" << endl;
	return;
}

void ExeDropTable(const std::string& tableName)
{
	Catalog* catalog = &Catalog::Instance();
	try{
		catalog->DropTable(tableName);
	}
	catch (const TableNotFound){
		cout << "Table `" << tableName << "` Not Found" << endl;
		return ;
	}
	cout << "Drop Table `" << tableName << "` Successfully" << endl;
	return;
}

void ExeCreateIndex(const std::string& tableName, const std::string& attrName, const string& indexName)
{
	Catalog* catalog = &Catalog::Instance();
	try{
		catalog->CreateIndex(indexName, tableName, attrName);
	}
	catch(const DuplicatedIndexName &){
		cout << "Duplicated Index Name `" << indexName << "`" << endl;
		return;
	}
	catch (const TableNotFound) {
		cout << "Table `" << tableName << "` Not Found" << endl;
		return;
	}
	catch (const AttributeNotFound) {
		cout << "Attribute `" << attrName << "` Not Found" << endl;
	}
	cout << "Create Index on `" << tableName << "` Successfully" << endl;
}

void ExeCreateTable(const std::string& tableName, const AttrDefinitionVector& defVec)
{
	Catalog* catalog = &Catalog::Instance();
	int attr_num = defVec.size();
	int key_index = -1;
	DBenum* attr_type_list = new DBenum[attr_num];
	string* attr_name_list = new string[attr_num];
	for(int i = 0; i < attr_num; i++){
		attr_name_list[i] = defVec[i].AttrName;
		if(defVec[i].TypeName == "int"){
			attr_type_list[i] = DB_TYPE_INT;
		}
		else if(defVec[i].TypeName == "float"){
			attr_type_list[i] = DB_TYPE_FLOAT;
		}
		else if(defVec[i].TypeName == "char"){
			attr_type_list[i] = (DBenum)(DB_TYPE_CHAR + defVec[i].TypeParam);
		}
		if(defVec[i].bePrimaryKey){
			key_index = i;
		}
	}
	try{
		catalog->CreateTable(tableName, attr_name_list, attr_type_list, attr_num, key_index);
	}
	catch (const DuplicatedTableName){
		cout << "Table Named `" << tableName << "` Already Existed" << endl;
		return;
	}
	cout << "Create Table `" << tableName << "` Successfully" <<endl;
	return;
}