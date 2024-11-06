#include "pch.h"
#include "Serialize.h"

#include "Typer.h"


namespace pbe {

   void Serializer::Ser(std::string_view name, TypeID typeID, const u8* value) {
      Typer::Get().Serialize((*this), name, typeID, value);
   }

   bool Serializer::SaveToFile(string_view filename) {
      std::ofstream fout{ filename.data() };
      fout << out.c_str();

      return true; // todo:
   }

   Deserializer Deserializer::FromFile(string_view filename) {
      std::ifstream fin{ filename.data() };
      return Deserializer{ YAML::Load(fin) };
   }

   Deserializer Deserializer::FromStr(string_view data) {
      return Deserializer{ YAML::Load(data.data()) };
   }

   bool Deserializer::Deser(std::string_view name, TypeID typeID, u8* value) const {
      return Typer::Get().Deserialize((*this), name, typeID, value);
   }

   Deserializer::Deserializer(YAML::Node node) : node(std::move(node)) {
   }
}
