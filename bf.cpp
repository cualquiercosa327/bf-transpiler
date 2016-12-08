#include <vector>
#include <iostream>
#include <memory>
using namespace std;

struct StatementList;
struct Statement {
	enum Type { ADD, MOVE, INPUT, OUTPUT, LOOP, ZERO, PROGRAM } type;
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
				ret->statements.push_back({Statement::ADD, offset, count, nullptr});
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

Statement optimizeZeroes(Statement program) {
	if (!program.body)
		return program;
	vector<Statement>& stmts = program.body->statements;
	if (program.type == Statement::LOOP && !program.body->moves && stmts.size() == 1) {
		const Statement& st = stmts.front();
		if (st.type == Statement::ADD && st.count % 2 != 0 && st.offset == program.offset)
			return {Statement::ZERO, program.offset, 1, nullptr};
	}
	for (auto& st : stmts)
		st = optimizeZeroes(move(st));
	return program;
}

void transpile(const Statement& st, ostream& os, int indent = 0) {
	auto line = [&]() -> ostream& {
		return os << '\n' << string(indent*2, ' ');
	};
	if (st.type == Statement::PROGRAM) {
		os << "#include <stdio.h>\n";
		os << "#include <stdint.h>\n";
		os << "\n";
		os << "int main(int argc, char** argv) {\n";
		os << "  uint8_t buffer[30000] = {0};\n";
		os << "  int pos = 0;";
		for (const Statement& st2 : st.body->statements)
			transpile(st2, os, indent + 1);
		os << "\n}\n" << flush;
	}
	else if (st.type == Statement::ADD) {
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
	else if (st.type == Statement::ZERO) {
		line() << "buffer[pos + " << st.offset << "] = 0;";
	}
	else if (st.type == Statement::LOOP) {
		line() << "while (buffer[pos + " << st.offset << "]) {";
		for (const Statement& st2 : st.body->statements)
			transpile(st2, os, indent + 1);
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
	program = optimizeZeroes(move(program));
	transpile(program, cout);
}
