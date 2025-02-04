#include "CSV_DataGenerator.h"

#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

using namespace iknow::csvdata;
using namespace std;

void str_subsitute(string& text, const string str_find, const string str_replace)
{
	string::size_type n = 0;
	while ((n = text.find(str_find, n)) != std::string::npos)
	{
		text.replace(n, str_find.size(), str_replace);
		n += str_replace.size();
	}
}

bool iKnow_KB_Rule::ImportFromCSV(string rules_csv, CSV_DataGenerator& kb)
{
	ifstream ifs = ifstream(rules_csv, ifstream::in);
	if (ifs.is_open()) {
		kb.handle_UTF8_BOM(ifs);

		newLabelsIndex_type newLabelsIndex; // mapping new labels 
		newLabels_type newLabels; // mapping labelName and phase to new label

		AutoGeneratedLabels_type auto_generated_labels = {
			Label_Phases(iKnow_KB_Label("SBegin", "typeAttribute"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SEnd", "typeAttribute"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("QBegin", "typeAttribute"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("QEnd", "typeAttribute"), SPhases_type({"1","$"})),

			Label_Phases(iKnow_KB_Label("SCertainty", "typeAttribute", "Entity(Certainty)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric1", "typeAttribute", "Entity(Generic1)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric1Begin", "typeAttribute", "Path(Begin,Generic1)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric1Stop", "typeAttribute", "Path(End,Generic1)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric2", "typeAttribute", "Entity(Generic2)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric2Begin", "typeAttribute", "Path(Begin,Generic2)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric2Stop", "typeAttribute", "Path(End,Generic2)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric3", "typeAttribute", "Entity(Generic3)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric3Begin", "typeAttribute", "Path(Begin,Generic3)"), SPhases_type({"1","$"})),
			Label_Phases(iKnow_KB_Label("SGeneric3Stop", "typeAttribute", "Path(End,Generic3)"), SPhases_type({"1","$"}))
		};

		int max_rule_csv_id = 0; // store the maximum rule id, to be able to generate a new, unique, one.
		vector<string> vec_rules_csv;
		set<int> rule_rewrite_indexes;
		set<int> rule_id_set; // store id's to prevent double use
		for (string line; getline(ifs, line);)
		{
			vec_rules_csv.push_back(line); // store in case we need to rewrite with new id's
			if (line.find("/*") != string::npos) continue; // Continue:$Find(line, "/*")'=0 // comment line
			if ((std::count(line.begin(), line.end(), ';') + 1) < 4) continue; // Continue : ($L(line, ";") < 4)

			vector<string> row_rule = kb.split_row(line);
			if (row_rule.size() == 4) { // no pending comment
				if (line[line.size() - 1] != ';') // last symbol must be terminating ';'
					throw ExceptionFrom<iKnow_KB_Rule>("Missing terminator symbol \";\" in line: " + line);
			}
			iKnow_KB_Rule rule; // Set rule = ..%New()
			rule.csv_id = row_rule[1 - 1]; // csv identification of rule
			int rule_csv_id = 0;
			if (!rule.csv_id.empty()) {
				rule_csv_id = std::stoi(rule.csv_id);
				if (rule_id_set.count(rule_csv_id)) { // rule ID already in use...
					throw ExceptionFrom<iKnow_KB_Rule>("ID Not unique in Rule : " + line);
				}
				else
					rule_id_set.insert(rule_csv_id);
			}
			if (rule_csv_id == 0) { // need a new id
				string& last_line = vec_rules_csv.back();

				string quot1("\xe2\x80\x9d"), quot2("\xe2\x80\x9c"); // rewrite exotic (�,�) qoutes.
				str_subsitute(last_line, quot1, "\"");
				str_subsitute(last_line, quot2, "\"");

				rule_rewrite_indexes.insert((int)vec_rules_csv.size() - 1); // keep the index
				row_rule = kb.split_row(last_line);
			}
			else {
				if (rule_csv_id > max_rule_csv_id) // store the bigger one
					max_rule_csv_id = rule_csv_id;
			}
			rule.Phase = row_rule[2 - 1]; // Set phase = $PIECE(line, ";", 2)	// Set rule.Phase = phase
			rule.InputPattern = rule.TransformRulePattern(row_rule[3 - 1], rule.Phase, kb, newLabels, newLabelsIndex, auto_generated_labels); // Set rule.InputPattern = ..TransformRulePattern($PIECE(line, ";", 3), phase, kb, .newLabels, .newLabelsIndex, .SBeginPhases, .SEndPhases)
			string rewrite_pattern = row_rule[4 - 1];
			if (*rewrite_pattern.rbegin() == '|') {
				throw ExceptionFrom<iKnow_KB_Rule>("Rule rewrite \"" + rewrite_pattern + "\" has empty end...");
			}
			rule.OutputPattern = kb.split_row(rewrite_pattern, '|'); // No need for transforming output patterns.
			if (rule.InputPattern.size() != rule.OutputPattern.size()) {
				throw ExceptionFrom<iKnow_KB_Rule>("Rule input (" + std::to_string(rule.InputPattern.size()) + ") & output pattern (" + std::to_string(rule.OutputPattern.size()) + ") items count do not match:\"" + line + "\"");
			}
			if (rule.Phase == "$") rule.Phase = "99"; // prevent last_phase rules to be selected first
			if (rule.Phase.empty())
				throw ExceptionFrom<iKnow_KB_Rule>("No Phase number defined in rule:\"" + line + "\"");

			rule.Precedence = (std::stoi(rule.Phase) * 100000) + (int)vec_rules_csv.size(); // sort on phasenumbers and count = appearance in file.

			kb.kb_rules.push_back(rule);
		}
		ifs.close(); // done reading
		if (rule_rewrite_indexes.size()) { // generate new indexes and rewrite...
			std::ofstream ofs = std::ofstream(rules_csv, ofstream::out);
			if (ofs.is_open()) {
				for (int idx = 0; idx < vec_rules_csv.size(); ++idx) {
					string& line = vec_rules_csv[idx];

					if (rule_rewrite_indexes.count(idx)) { // needs rewrite
						max_rule_csv_id++; // next index
						ofs << max_rule_csv_id << line << endl;
					}
					else { // copy line
						ofs << line << endl;
					}
				}
				ofs.close();
			}
		}
		// sort kb_rules on Precedence
		std::sort(kb.kb_rules.begin(), kb.kb_rules.end(), [](iKnow_KB_Rule const& a, iKnow_KB_Rule const& b) { return a.Precedence < b.Precedence;  });

		// typedef std::vector<std::pair<std::string, std::string> > newLabels_type;
		for (newLabels_type::iterator itLabel = newLabels.begin(); itLabel != newLabels.end(); ++itLabel) // Set key = $ORDER(newLabels(""))	// While key '= "" {	// Set key = $ORDER(newLabels(key))
		{
			iKnow_KB_Label labelObj(itLabel->first, "typeLiteral"); // Set labelObj.Type = "typeLiteral"
			labelObj.PhaseList = itLabel->second + ",$"; // Set labelObj.PhaseList = $List(newLabels(key), 2) _ ",$" // add last phase
			kb.kb_labels.push_back(labelObj); // Set sc = labelObj.%Save()
		}

		// handle and store auto-generated labels
		for (auto it_label = auto_generated_labels.begin(); it_label != auto_generated_labels.end(); ++it_label) {
			iKnow_KB_Label& label = it_label->first;

			for_each(it_label->second.begin(), it_label->second.end(), [&label](string phase) { label.PhaseList += (label.PhaseList.size() ? "," + phase : phase); });
			kb.kb_labels.push_back(label); // Set sc = labelObj.%Save()
		}

		return true;
	}
	cerr << "Error opening file: " << rules_csv << " Language=\"" << kb.GetName() << "\"" << endl;
	return false;
}

void AddLabelToLexrep(CSV_DataGenerator& kb, string& token, string& label)
{
	// escape the special chars.
	str_subsitute(token, string("("), string("\\("));
	str_subsitute(token, string("{"), string("\\{"));

	// Full lexrep addition
	auto it_lexrep = kb.lexrep_index.find(token); // &sql(select ID into : lexrepId from Lexrep where Token = : token and Knowledgebase = : kbId)
	if (it_lexrep != kb.lexrep_index.end()) { // If(SQLCODE = 0) && lexrepId{
		int lexrep_index = it_lexrep->second;
		iKnow_KB_Lexrep &lexrep = kb.kb_lexreps[lexrep_index]; // Set lexrep = ##class(Lexrep).%OpenId(lexrepId)
		string separator = (*(lexrep.Labels.end() - 1) == ';' ? "" : ";"); // Set separator = $select($extract(lexrep.Labels, *) = ";":"", 1 : ";") // label separator
		if (lexrep.Labels.find(label) == string::npos)
			lexrep.Labels = lexrep.Labels + separator + label + ";"; // // If(lexrep.Labels '[ label) Set lexrep.Labels = lexrep.Labels _ separator _ label _ ";" // check for lexrep.Labels ending '; '
	}
	else { // new
		iKnow_KB_Lexrep lexrep; // Set lexrep = ##class(Lexrep).%New()
		lexrep.Token = token; // Set lexrep.Token = token
		lexrep.Labels = label + ";"; // Set lexrep.Labels = label _ ";"
		kb.lexrep_index[lexrep.Token] = (int) kb.kb_lexreps.size(); // new index for lexrep
		kb.kb_lexreps.push_back(lexrep); // Set sc = lexrep.%Save()
	}
	// Lexrep segment addition
	auto it_lexrep_segment = kb.lexrep_segments_index.find(token);
	if (it_lexrep_segment != kb.lexrep_segments_index.end()) { // token represents a lexrep segment
		vector<int>& segment_indexes = kb.lexrep_segments_index[token];

		for (auto it_index = segment_indexes.begin(); it_index != segment_indexes.end(); ++it_index) {
			int lexrep_index = *it_index++;
			int segment_index = *it_index;

			iKnow_KB_Lexrep& lexrep = kb.kb_lexreps[lexrep_index]; // Set lexrep = ##class(Lexrep).%OpenId(lexrepId)
			vector<string> label_segments, list_labels = kb.split_row(lexrep.Labels, ';');
			string label_segment;
			for (auto it = list_labels.begin(); it != list_labels.end(); it++) {
				if (*it == "-") { // segment splitter
					label_segments.push_back(label_segment);
					label_segment.clear();
					continue;
				}
				label_segment += (*it + ";");
			}
			label_segments.push_back(label_segment);

			string& token_label_segment = label_segments[segment_index];
			if (token_label_segment.find(label) == string::npos) { // add label if not present
				token_label_segment += label + ";";
			}
			string labels_update;
			for (auto it = label_segments.begin(); it < label_segments.end(); ++it) {
				labels_update += *it;
				if (it < label_segments.end() - 1)
					labels_update += "-;";
			}
			// for_each(label_segments.begin(), label_segments.end(), [&labels_update](string& label_segment) { labels_update += label_segment + "-"; });
			lexrep.Labels = labels_update; // updated with literal label
		}
	}
	// kb.lexrep_segments_index[token_segment].push_back(idx_lexrep)
}

void iKnow_KB_Rule::ProcessRuleOutputPattern(string& pattern, string& phase, CSV_DataGenerator& kb, AutoGeneratedLabels_type& AutoGeneratedLabels)
{
	vector<string> labels = kb.split_row(pattern, '|');

	for (vector<string>::iterator it = labels.begin(); it != labels.end(); ++it) {
		string& label = *it;

		// collect phase numbers for autogenerated labels
		for (auto it_label = AutoGeneratedLabels.begin(); it_label != AutoGeneratedLabels.end(); ++it_label) { // scan the autogen list
			if (label.find(it_label->first.Name) != string::npos) // auto gen label in rule labels list
				it_label->second.insert(phase); // collect the phase
		}
	}
}
vector<string> iKnow_KB_Rule::TransformRulePattern(string& pattern, string& phase, CSV_DataGenerator& kb, newLabels_type &newLabels, newLabelsIndex_type &newLabelsIndex, AutoGeneratedLabels_type &AutoGeneratedLabels)
{
	// Set len = $L(pattern, "|")
	vector<string> labels = kb.split_row(pattern, '|');
	// size_t len = labels.size();
	for (vector<string>::iterator it = labels.begin(); it != labels.end(); ++it) {
		string& label = *it;

		// collect phase numbers for autogenerated labels
		for (auto it_label = AutoGeneratedLabels.begin(); it_label != AutoGeneratedLabels.end(); ++it_label) { // scan the autogen list
			if (label.find(it_label->first.Name) != string::npos) // auto gen label in rule labels list
				it_label->second.insert(phase); // collect the phase
		}
		if (label.find("\"") != string::npos) { // convert literal to literal label
			size_t n = std::count(label.begin(), label.end(), '\"');
			if (n % 2) { // odd number of quotes in literal label
				throw ExceptionFrom<iKnow_KB_Rule>("Illegal formed literal label: |" + label + "|");
			}
			string newLabel;
			int lastQuote = 0;
			for (string::iterator ic = label.begin(); ic != label.end(); ++ic) { // scan the label selector
				char c = *ic;
				if (lastQuote) {
					if (c == '\"') {
						string token(label.begin() + lastQuote, ic); // Set token = $E(label, lastQuote + 1, j - 1)
						string labelName = "Lit_" + token; // labelname constructed out of token = literal label
						str_subsitute(labelName, string(":"), string("DoublePoints")); // Set labelName = "Lit_" _ $replace($replace($replace(token, ":", "DoublePoints"), "+", "plus"), "(", "lbrack") // Labels cannot contain the or ':' or '+' symbol
						str_subsitute(labelName, string("+"), string("plus"));
						str_subsitute(labelName, string("("), string("lbrack"));

						if (newLabelsIndex.find(labelName) != newLabelsIndex.end()) { // If $Data(newLabelsIndex(labelName)) { // Literal label exist already
							int idxLabel = newLabelsIndex[labelName]; // Set idxLabel = newLabelsIndex(labelName)
							string rulePhases = newLabels[idxLabel].second; // Set rulePhases = $List(newLabels(idxLabel), 2)
							if (rulePhases.find(phase) == string::npos) // Set:rulePhases'[phase $List(newLabels(idxLabel),2) = rulePhases_","_phase
								rulePhases += ("," + phase);
							newLabels[idxLabel].second = rulePhases; // overwrite
						}
						else { // New label
							newLabels.push_back(make_pair(labelName, phase)); // Set newLabels($Increment(newLabels)) = $Lb(labelName, phase) // store name using a numeric key to avoid collision problems
							newLabelsIndex.insert(make_pair(labelName, static_cast<int>(newLabels.size() - 1))); // Set newLabelsIndex(labelName) = newLabels
						}

						AddLabelToLexrep(kb, token, labelName); // Do ..AddLabelToLexrep(kb, token, labelName)
						//Rewrite label pattern string
						newLabel = newLabel + labelName; // Set newLabel = newLabel _ labelName
						lastQuote = 0; // Set lastQuote = 0
					}
				}
				else {
					if (c == '\"')
						lastQuote = static_cast<int>(ic - label.begin()) + 1;
					else
						newLabel += *ic;
				}
			}
			label = newLabel; // overwrite csv-label
		}
	}
	return labels;

	/*
	string transformed_pattern;
	for (vector<string>::iterator it = labels.begin(); it != labels.end(); ++it) { // reconstruct output
		if (it != labels.begin()) transformed_pattern += "|";
		transformed_pattern += *it;
	}
	return transformed_pattern;
	*/
}