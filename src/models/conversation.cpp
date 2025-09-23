#include "models/conversation.hpp"

#include <fstream>
#include <iostream>

namespace llm {

void Conversation::save_to_file(const std::string &filename) const {
  try {
    std::ofstream file(filename);
    if (!file.is_open()) {
      std::cout << "[ERROR] Failed to open file for writing: " << filename
                << "\n";
      return;
    }

    file << to_json().dump(2);
    file.close();

    std::cout << "[INFO] Conversation saved to " << filename << "\n";
  } catch (const std::exception &e) {
    std::cout << "[ERROR] Error saving conversation: " << e.what() << "\n";
  }
}

void Conversation::load_from_file(const std::string &filename) {
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cout << "[ERROR] Failed to open file for reading: " << filename
                << "\n";
      return;
    }

    nlohmann::json j;
    file >> j;
    file.close();

    from_json(j);
    std::cout << "[INFO] Conversation loaded from " << filename << "\n";
  } catch (const std::exception &e) {
    std::cout << "[ERROR] Error loading conversation: " << e.what() << "\n";
  }
}

} // namespace llm