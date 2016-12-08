#include <vector>
#include <iostream>
#include <memory>
using namespace std;

struct StatementList;
struct Statement {
	enum Type { INCR, MOVE, INPUT, OUTPUT, LOOP, ADD, PROGRAM } type;
	int offset, count;
	unique_ptr<StatementList> body;
};

struct StatementList {
	int moves = false;
	vector<Statement> statements;
};

unique_ptr<StatementList> parse(istream& is, int baseOffset) {
	unique_ptr<StatementList> ret(new StatementList());
	int currentOffset = baseOffset;
	map<int, int> adds;

	auto pushAdds = [&]() {
		for (const auto& pa : adds) {
			int offset = pa.first, count = pa.second;
			if (count != 0)
				ret->statements.push_back({Statement::INCR, offset, count, nullptr});
		}
		adds.clear();
	};

	char ch;
	while (is.peek() != ']' && is.get(ch)) {
		if (ch == '+') {
			adds[currentOffset]++;
		}
		else if (ch == '-') {
			adds[currentOffset]--;
		}
		else if (ch == '>') {
			currentOffset++;
		}
		else if (ch == '<') {
			currentOffset--;
		}
		else if (ch == '.') {
			pushAdds();
			ret->statements.push_back({Statement::OUTPUT, currentOffset, 1, nullptr});
		}
		else if (ch == ',') {
			pushAdds();
			ret->statements.push_back({Statement::INPUT, currentOffset, 1, nullptr});
		}
		else if (ch == '[') {
			pushAdds();
			Statement st = {Statement::LOOP, currentOffset, 1, nullptr};
			st.body = parse(is, currentOffset);
			ret->moves |= st.body->moves;
			ret->statements.push_back(move(st));
			if (is.get() != ']')
				throw string("Opening [ with no closing");
		}
	}
	pushAdds();

	if (currentOffset != baseOffset) {
		ret->moves = true;
		ret->statements.push_back({Statement::MOVE, 0, currentOffset - baseOffset, nullptr});
	};
	return ret;
}

Statement parseProgram(istream& is) {
	unique_ptr<StatementList> body = parse(is, 0);
	if (is.get() == ']')
		throw string("Closing ] with no opening");
	return {Statement::PROGRAM, 0, 1, move(body)};
}

pair<int, int> minMaxOffsets(const Statement& st) {
	pair<int, int> ret{st.offset, st.offset};
	if (st.body) {
		for (const Statement& st2 : st.body->statements) {
			auto pa = minMaxOffsets(st2);
			ret.first = min(ret.first, pa.first);
			ret.second = max(ret.second, pa.first);
		}
	}
	return ret;
}

Statement optimizeAdds(Statement st) {
	if (!st.body) return st;
	vector<Statement>& stmts = st.body->statements;
	for (auto& st2 : stmts)
		st2 = optimizeAdds(move(st2));

	if (st.type != Statement::LOOP || st.body->moves) return st;
	int loopIncr = 0;
	for (const Statement& st2 : stmts) {
		if (st2.type != Statement::INCR) return st;
		if (st2.offset == st.offset) loopIncr += st2.count;
	}
	if (abs(loopIncr) != 1) return st;
	if (loopIncr == 1) {
		for (Statement& st2 : stmts)
			st2.offset = -st2.offset;
		loopIncr = -1;
	}
	return {Statement::ADD, st.offset, 1, move(st.body)};
}

void transpile(const Statement& st, ostream& os, int& idcount, int indent = 0) {
	auto line = [&]() -> ostream& {
		return os << '\n' << string(indent*2, ' ');
	};
	if (st.type == Statement::PROGRAM) {
		pair<int, int> offsets = minMaxOffsets(st);
		int bufSize = 30000 + offsets.second - offsets.first;
		os << "#include <stdio.h>\n";
		os << "#include <stdint.h>\n";
		os << "\n";
		os << "int main(int argc, char** argv) {\n";
		os << "  uint8_t buffer[" << bufSize << "] = {0};\n";
		os << "  int pos = " << -offsets.first << ";";
		for (const Statement& st2 : st.body->statements)
			transpile(st2, os, idcount, indent + 1);
		os << "\n}\n" << flush;
	}
	else if (st.type == Statement::INCR) {
		line() << "buffer[pos + " << st.offset << "] += " << st.count << ";";
	}
	else if (st.type == Statement::MOVE) {
		line() << "pos += " << st.count << ";";
	}
	else if (st.type == Statement::INPUT) {
		line() << "buffer[pos + " << st.offset << "] = getchar();";
	}
	else if (st.type == Statement::OUTPUT) {
		line() << "putchar(buffer[pos + " << st.offset << "]);";
	}
	else if (st.type == Statement::ADD) {
		int id = -1;
		if (st.body->statements.size() > 1) {
			id = idcount++;
			line() << "int tmp" << id << " = buffer[pos + " << st.offset << "];";
		}
		line() << "buffer[pos + " << st.offset << "] = 0;";
		for (const Statement& st2 : st.body->statements) {
			if (st2.offset == st.offset) continue;
			assert(id != -1);
			line() << "buffer[pos + " << st2.offset << "] += tmp" << id << " * " << st2.count << ";";
		}
	}
	else if (st.type == Statement::LOOP) {
		line() << "while (buffer[pos + " << st.offset << "]) {";
		for (const Statement& st2 : st.body->statements)
			transpile(st2, os, idcount, indent + 1);
		line() << "}";
	}
	else assert(0);
}

int main(int argc, char** argv) {
	Statement program;
	try {
		program = parseProgram(cin);
	} catch (const string& err) {
		cerr << "Parse error: " << err << endl;
		return 1;
	}
	program = optimizeAdds(move(program));
	int idcount = 0;
	transpile(program, cout, idcount);
}
