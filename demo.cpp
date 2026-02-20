#include "hashmap.hpp"

#include <iostream>

int main()
{
    // Create a HashMap with string keys and int values
    optimap::HashMap<std::string, int> map;

    std::cout << "Demonstrating OptiMap HashMap" << std::endl;
    std::cout << "==============================" << std::endl;

    // Insert some elements
    std::cout << "\nInserting elements..." << std::endl;
    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);
    std::cout << "Map size: " << map.size() << std::endl;

    // Find and print elements
    std::cout << "\nFinding elements..." << std::endl;
    auto val1 = map.find("two");
    if (val1 != map.end())
    {
        std::cout << "Found key 'two' with value: " << val1->second << std::endl;
    }

    auto val2 = map.find("four");
    if (val2 == map.end())
    {
        std::cout << "Key 'four' not found, as expected." << std::endl;
    }

    // Use iterators to print all elements
    std::cout << "\nIterating over all elements:" << std::endl;
    for (const auto& pair : map)
    {
        std::cout << "- {" << pair.first << ": " << pair.second << "}" << std::endl;
    }

    // Demonstrate erase
    std::cout << "\nErasing 'one'..." << std::endl;
    map.erase("one");
    std::cout << "Map size after erase: " << map.size() << std::endl;

    std::cout << "\nFinal map contents:" << std::endl;
    for (const auto& pair : map)
    {
        std::cout << "- {" << pair.first << ": " << pair.second << "}" << std::endl;
    }

    std::cout << "\nDemonstration complete." << std::endl;

    return 0;
}
