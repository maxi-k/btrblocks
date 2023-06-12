#include "common/Units.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
#include <regex>
#include <set>
#include <map>
#include <fstream>
// -------------------------------------------------------------------------------------
DEFINE_bool(print_header, false, ".");
DEFINE_string(in, "", ".");
DEFINE_string(out_csv, "", ".");
DEFINE_string(delimiter, "\t", ".");
DEFINE_uint32(max_char_per_cell, 100, ".");
DEFINE_uint32(block_print_length, 20, ".");
DEFINE_uint32(block_count, 3, ".");
DEFINE_uint32(block_length, 65000, ".");
using namespace std;
using namespace btrblocks;
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
map<string, string> analyzeBlock(Vector<INTEGER> &column, Vector<BITMAP> &bitmap, u32 start_index, u32 tuple_count, bool print_block = false)
{
   map<string, string> stats;
   stats["random_element"] = to_string(column[start_index]);
   INTEGER min, max;
   bool is_starting_values_init = false;
   u32 null_count = 0;
   u32 zero_count = 0;
   std::unordered_map<INTEGER, u32> frequency;

   for ( u32 tuple_i = start_index; tuple_i < start_index + tuple_count; tuple_i++ ) {
      BITMAP is_set = bitmap.data[tuple_i];

      if ( !is_set ) {
         null_count++;
         continue;
      }

      auto current_value = column[tuple_i];
      if ( current_value == 0 ) {
         zero_count++;
      }

      if ( frequency.find(current_value) == frequency.end()) {
         frequency.insert({current_value, 1});
      } else {
         frequency[current_value] = frequency[current_value] + 1;
      }

      if(is_starting_values_init) {
         if ( current_value > max )
            max = current_value;
         if ( current_value < min)
            min = current_value;
      } else {
         is_starting_values_init = true;
         min = max = current_value;
      }

   }
   const u32 set_count = tuple_count - null_count;
   const u32 unique_count = frequency.size();
   {
      using Comparator = function<bool(pair<INTEGER, u32>, pair<INTEGER, u32>)>;
      // Defining a lambda function to compare two pairs. It will compare two pairs using second field
      Comparator compFunctor =
        [](pair<INTEGER, u32> elem1, pair<INTEGER, u32> elem2) {
          return elem1.second > elem2.second;
        };
      // Declaring a set that will store the pairs using above comparision logic
      set<pair<INTEGER, u32>, Comparator> frequency_set(frequency.begin(), frequency.end(), compFunctor);
      u32 top_i = 1;
      for ( const auto &element: frequency_set ) {
         INTEGER value = element.first;
         double frequency = static_cast<double>(element.second) * 100.0 / static_cast<double>(set_count);
         string key_prefix = "top_" + to_string(top_i);
         string value_key = key_prefix + "_value";
         string percent_key = key_prefix + "_percent";

         stats[value_key] = to_string(value);
         stats[percent_key] = to_string(frequency);
         if ( top_i++ == ((unique_count >= 3) ? 3 : unique_count)) {
            break;
         }
      }
      for ( ; top_i <= 3; top_i++ ) {
         string key_prefix = "top_" + to_string(top_i);
         string value_key = key_prefix + "_value";
         string percent_key = key_prefix + "_percent";

         stats[value_key] = "";
         stats[percent_key] = "";
      }
   }

   stats["min"] = to_string(min);
   stats["max"] = to_string(max);
   stats["null_count"] = to_string(null_count);
   stats["zero_count"] = to_string(zero_count);
   stats["unique_count"] = to_string(unique_count);
   // -------------------------------------------------------------------------------------
   if ( print_block ) {
      string block_rep = "";
      for ( u32 tuple_i = start_index + 1; tuple_i < start_index + FLAGS_block_print_length; tuple_i++ ) {
         BITMAP is_set = bitmap.data[tuple_i];
         if ( !is_set ) {
            block_rep += "N";
         } else if ( column[tuple_i] == column[tuple_i - 1] ) {
            block_rep += ".";
         } else {
            block_rep += "x";
         }
      }
      stats["block"] = block_rep;
   }
   // -------------------------------------------------------------------------------------
   return stats;
}
int main(int argc, char **argv)
{
   srand(time(NULL));
   // -------------------------------------------------------------------------------------
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------
   assert(FLAGS_out_csv.size());
   string data_file = FLAGS_in, bitmap_file;
   {
      std::regex re("(.*).integer");
      std::smatch match;
      if ( std::regex_search(data_file, match, re) && match.size() > 1 ) {
         bitmap_file = match.str(1) + ".bitmap";
      }
   }
   // -------------------------------------------------------------------------------------
   Vector<INTEGER> column;
   column.readBinary(data_file.c_str());
   Vector<BITMAP> bitmap;
   bitmap.readBinary(bitmap_file.c_str());
   auto tuple_count = bitmap.size();
   assert(bitmap.size() == column.size());
   // -------------------------------------------------------------------------------------
   map<string, string> stats;
   stats["col_count"] = to_string(column.size());
   {
      std::regex re("(\\/[^\\/]+\\/[^\\/]+).integer");
      std::smatch match;
      if ( std::regex_search(data_file, match, re) && match.size() > 1 ) {
         stats["col_id"] = '"' + match.str(1) + '"';
      }
   }
   // -------------------------------------------------------------------------------------
   auto whole_column = analyzeBlock(column, bitmap, 0, tuple_count);
   for ( const auto &element: whole_column ) {
      stats["col_" + element.first] = element.second;
   }
   // -------------------------------------------------------------------------------------
   for ( u32 block_i = 1; block_i <= FLAGS_block_count; block_i++ ) {
      u32 start_index = rand() % tuple_count;
      u32 block_length = std::min(FLAGS_block_length, static_cast<u32>(tuple_count - start_index));
      auto block = analyzeBlock(column, bitmap, start_index, block_length, true);
      for ( const auto &element: block ) {
         stats["block_" + to_string(block_i) + "_" + element.first] = element.second;
      }
   }
   // -------------------------------------------------------------------------------------

   std::ofstream csv;
   csv.open(FLAGS_out_csv, std::ofstream::out | std::ofstream::app);
   assert(csv.good());
   if ( csv.tellp() == 0 ) {
      for ( auto it = stats.begin(); it != stats.end(); ) {
         csv << it->first;
         if ( ++it != stats.end()) {
            csv << FLAGS_delimiter;
         }
      }
      csv << endl;
   }
   for ( auto it = stats.begin(); it != stats.end(); ) {
      auto sub_str = it->second.substr(0, FLAGS_max_char_per_cell);
      std::regex tabs_regex("\\t");
      std::regex nl_regex("\\n");
      auto sterilized_value = std::regex_replace(sub_str, tabs_regex, " ");
      sterilized_value = std::regex_replace(sterilized_value, nl_regex, " ");

      csv << sterilized_value;
      if ( ++it != stats.end()) {
         csv << FLAGS_delimiter;
      }
   }
   csv << endl;
   // -------------------------------------------------------------------------------------
   return 0;
}
// -------------------------------------------------------------------------------------
