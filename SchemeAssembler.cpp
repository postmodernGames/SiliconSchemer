#include <algorithm>
#include <fstream>
#include  <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <utility>
using namespace std;

using byte =uint8_t;
using word =uint16_t;
using tokenPair =pair<int,string>;
using tokenType = pair<int, string>;

const int EOFCode = -1, ListCode=0, BuiltinCode = 1, OrdAtomCode = 2, NumberCode=3, NilCode=4, whitespaceCode=5;

map<string,word> builtinFunctionTable;
map<string, word> atomTable;
word RAM[400];
string listBuiltins[] = { "car","cdr","cons","atom","null","lambda","cond","eq","nil","t"};

ifstream infile;

string str_toupper(string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return std::toupper(c); } 
	);
	return s;
}


void insertBuiltin(string func) {
	static unsigned int index = 0;
	word modifiedIndex = (index & 0x3fff) | 0x4000;
	builtinFunctionTable.insert(make_pair(func, modifiedIndex));
	index++;
}

void initialize() {
	for (auto& func : listBuiltins) {
		func = str_toupper(func);
		insertBuiltin(func);
	}
}


int tokenTypeDeterminer(char s){
	string c = "";
	c += s;
	regex whitespace("[[:s:]]|\n");
	regex ordatom("[[:alpha:]]|[$]");
	regex number("[[:digit:]]");
	regex openParen("[(]");
	regex closeParen("[)]");

	if(regex_match(c,whitespace))  return whitespaceCode;
	else if(regex_match(c,ordatom))  return OrdAtomCode;
	else if(regex_match(c,number)) return NumberCode;
	else if(regex_match(c,openParen)) return ListCode;
	else if(regex_match(c,closeParen)) return NilCode;
	return -1;
}


word makeAtom(string token) {
	static word index = 0;
	auto it = builtinFunctionTable.find(str_toupper(token));
	if (it != builtinFunctionTable.end()) {
		return it->second;
	}
	it = atomTable.find(token);
	if (it != atomTable.end()) {
		return it->second;
	}
	word modifiedIndex = (index & 0x3fff) | 0x8000;
	atomTable.insert(make_pair(token, modifiedIndex));
	index++;
	return atomTable[token];
}

word makeNumber(string token){
	unsigned int num;
	num = stoi(token);
	return (0x3fff & num) | 0xc000; 
}
	
tokenType scan() {
	string s = "";
	int d = 0;

	int type, lastType = -2;
	string token;

	char c = infile.get();
	type = tokenTypeDeterminer(c);
	while (infile.good() && type == whitespaceCode) {
		c = infile.get();
		type = tokenTypeDeterminer(c);
	}
	while (infile.good()) {
		if (type == ListCode || type == whitespaceCode || type == NilCode) {
			if (lastType == OrdAtomCode) {
				token = s;
				s = "";
				infile.putback(c);

			}
			else if (lastType == NumberCode) {
				token = d;
				d = 0;
				infile.putback(c);
			}
			else if (type == ListCode) {
				token = "(";
				lastType = ListCode;
				
			}
			else if (type == NilCode) {
				token = ")";
				lastType = NilCode;
			}
			if (lastType != whitespaceCode) {
				if (lastType == OrdAtomCode) {
					if (builtinFunctionTable.find(token) != builtinFunctionTable.end()) lastType = BuiltinCode;
				}
				return make_pair(lastType, token);
			}
			lastType = -2;  //I need to track lastType to know whether s or d have a token culminating.  -2 indicates that no token is brewing


		}
		else if (type == OrdAtomCode) {
			s += c;
			lastType = OrdAtomCode;
		}
		else if (type == NumberCode) {
			d = 10 * d + c - '0'; //need atoi
			lastType = NumberCode;
		}


		c = infile.get();
		type = tokenTypeDeterminer(c);

	}
	return make_pair(-1, "EOF");
};

word encodeToken(tokenPair tt) {
	int  tokenType;
	string token;
	word encodedToken = -1;
	tokenType = tt.first;
	token = tt.second;
	if (tokenType == ListCode) {
		encodedToken = ListCode;
	}
	else if (tokenType == NilCode) {
		encodedToken = NilCode;
	}
	else if (tokenType == OrdAtomCode || tokenType == BuiltinCode) {
		encodedToken = makeAtom(token);
	}
	else if (tokenType == NumberCode)
	{
		encodedToken = makeNumber(token);
	}
	return encodedToken;

}

word getNewBlock() {
	static word address = 0;
	word newAddress = address;
	address += 2;
	return newAddress;
}

void readList(word address,pair<int,string> tokenPair) {
	
	int  tokenType, nextTokenType;
	string token, nextToken; 
	tokenType = tokenPair.first;
	token = tokenPair.second;
	word encodedToken;
	word nextAddress;

	while (tokenType!=NilCode) {

		auto nextTokenPair = scan();
		nextTokenType = nextTokenPair.first;
		nextToken = nextTokenPair.second;

		
		if (tokenType == ListCode) {

			nextAddress = getNewBlock();
			RAM[address] = nextAddress; 
			//cout << tokenPair.second << endl;
			readList(nextAddress, nextTokenPair);
			
			nextTokenPair = scan();
			nextTokenType = nextTokenPair.first;
			nextToken = nextTokenPair.second;
			if (nextTokenType != NilCode) {
				nextAddress = getNewBlock();
				RAM[address + 1] = nextAddress;
				address = nextAddress;
			};
			//cout << ')' << endl;
		
		}
		else if (tokenType == OrdAtomCode || tokenType == BuiltinCode) {
			encodedToken = encodeToken(tokenPair);
			RAM[address] = encodedToken;
			if (nextTokenType != NilCode && nextTokenType != EOFCode) {
				nextAddress = getNewBlock();
				RAM[address + 1] = nextAddress;
				address = nextAddress;
			}
			//cout << tokenPair.second << endl;
		}
		
		tokenPair = nextTokenPair;
		tokenType = tokenPair.first;
		token = tokenPair.second;
	}
}



int main(int argc, char** argv) {
	infile.open(argv[1]);
	ofstream outfile(argv[2], ios::out | ios::binary);
	int  tokenType, nextTokenType;
	string token, nextToken;

	initialize();

	word encodedToken;
	auto tt = scan();  //tt  = (type,token)
	tokenType = tt.first;
	token = tt.second;
	word address = 0, nextAddress;
	address = getNewBlock();
	RAM[address] = 0;
	RAM[address + 1] = 0;


	while (tt.first != -1) {
		auto nextTokenPair = scan();
		nextTokenType = nextTokenPair.first;
		nextToken = nextTokenPair.second;

		encodedToken = encodeToken(tt);
		if (tokenType == ListCode) {

			nextAddress = getNewBlock();
			RAM[address] = nextAddress;
			readList(nextAddress, nextTokenPair);
			nextTokenPair = scan();
			nextTokenType = nextTokenPair.first;
			nextToken = nextTokenPair.second;
			if (nextTokenType != NilCode  && nextTokenType != EOFCode) {
				nextAddress = getNewBlock();
				RAM[address + 1] = nextAddress;
				address = nextAddress;

			}
		}
		else if (tokenType == BuiltinCode) {
			RAM[address] = encodedToken;
		}
		else if (tokenType == OrdAtomCode) {
			RAM[address] = encodedToken;

		}

		//now that I've read in a list, the nextToken pair has changed
		tt = nextTokenPair;
		tokenType = tt.first;
		token = tt.second;
	}

	word lastAddress = getNewBlock();

	//cout << endl;
	string code,car;
	/*
	for (auto it = atomTable.begin(); it != atomTable.end(); ++it) {
		cout <<  it->first << '\t' << (it->second & 0x3fff) << endl;
	};*/ 
	/*cout << endl;
	for (int address = 0; address < lastAddress; address += 2) {
		if ((RAM[address] >> 14) == 0) {
			cout << address << '\t' << RAM[address] << '\t' << dec << RAM[address + 1] << '\t' << endl;
		}
		else {
			if ((RAM[address] >> 14) == 1) code = listBuiltins[RAM[address] & 0x3fff];
			else code = "";
			cout << address << '\t' << hex << RAM[address] << '\t' << dec << RAM[address + 1] << '\t' << code << endl;
		}
	}*/
	/*for (int address = 0; address < lastAddress; address++) {

		outfile << setfill('0') << setw(2) << right << hex << ((RAM[address]&0xff00) >> 8) << endl;
		outfile << setfill('0') << setw(2) << right << hex << (RAM[address] & 0x00ff) << endl;
	};
	*/

	char* buffer;

	for (int address = 0; address < lastAddress; address++) {
		byte b = (byte)((RAM[address] & 0xff00) >> 8);
		buffer = (char*)&b;
		outfile.write(buffer,1);
		b = (byte)((RAM[address] & 0x00ff));
		buffer = (char*)&b;
		outfile.write(buffer,1);
	};

	infile.close();
	outfile.close();
	return 0;
}