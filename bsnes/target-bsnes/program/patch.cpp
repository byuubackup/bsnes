auto Program::appliedPatch() const -> bool {
  return (
     superFamicom.patched
  || gameBoy.patched
  || bsMemory.patched
  || sufamiTurboA.patched
  || sufamiTurboB.patched
  );
}

auto Program::applyPatchIPS(vector<uint8_t>& data, string location) -> bool {
  vector<uint8_t> patch;

  if(location.endsWith("/")) {
    patch = file::read({location, "patch.ips"});
  } else if(location.iendsWith(".zip")) {
    Decode::ZIP archive;
    if(archive.open(location)) {
      for(auto& file : archive.file) {
        if(file.name.iendsWith(".ips")) {
          patch = archive.extract(file);
          break;
        }
      }
    }
    if(!patch) patch = file::read(path("Patches", location, ".ips"));
  } else {
    patch = file::read(path("Patches", location, ".ips"));
  }
  if(!patch) return false;

  bool headered = false;
  if(MessageDialog().setAlignment(*presentation).setTitle({Location::prefix(location), ".ips"}).setText({
    "(You're seeing this prompt because IPS is a terrible patch file format,\n"
    " and nobody can agree on whether SNES ROMs should be headered or not.\n"
    " Please consider asking the patch author to use BPS patches instead.)\n\n"
    "Does this IPS patch expect to be applied to a headered ROM?\n"
    "If you're not sure, try 'No', and if it fails to work, try again with 'Yes'."
  }).question() == "Yes") headered = true;

  //sanity checks
  if(patch.size() < 8) return false;
  if(patch[0] != 'P') return false;
  if(patch[1] != 'A') return false;
  if(patch[2] != 'T') return false;
  if(patch[3] != 'C') return false;
  if(patch[4] != 'H') return false;

  for(uint index = 5;;) {
    if(index == patch.size() - 6) {
      if(patch[index + 0] == 'E' && patch[index + 1] == 'O' && patch[index + 2] == 'F') {
        uint32_t truncate = 0;
        truncate |= patch[index + 3] << 16;
        truncate |= patch[index + 4] <<  8;
        truncate |= patch[index + 5] <<  0;
        data.resize(truncate);
        return true;
      }
    }

    if(index == patch.size() - 3) {
      if(patch[index + 0] == 'E' && patch[index + 1] == 'O' && patch[index + 2] == 'F') {
        return true;
      }
    }

    if(index >= patch.size()) break;

    int32_t offset = 0;
    offset |= patch(index++, 0) << 16;
    offset |= patch(index++, 0) <<  8;
    offset |= patch(index++, 0) <<  0;
    if(headered) offset -= 512;

    uint16_t length = 0;
    length |= patch(index++, 0) << 8;
    length |= patch(index++, 0) << 0;

    if(length == 0) {
      uint16_t repeat = 0;
      repeat |= patch(index++, 0) << 8;
      repeat |= patch(index++, 0) << 0;

      uint8_t fill = patch(index++, 0);

      while(repeat--) {
        if(offset >= 0) data(offset) = fill;
        offset++;
      }
    } else {
      while(length--) {
        if(offset >= 0) data(offset) = patch(index, 0);
        offset++;
        index++;
      }
    }
  }

  //"EOF" marker not found in correct place
  //technically should return false, but be permissive (data was already modified)
  return true;
}

#include <nall/beat/single/apply.hpp>

auto Program::applyPatchBPS(vector<uint8_t>& input, string location) -> bool {
  vector<uint8_t> patch;

  if(location.endsWith("/")) {
    patch = file::read({location, "patch.bps"});
  } else if(location.iendsWith(".zip")) {
    Decode::ZIP archive;
    if(archive.open(location)) {
      for(auto& file : archive.file) {
        if(file.name.iendsWith(".bps")) {
          patch = archive.extract(file);
          break;
        }
      }
    }
    if(!patch) patch = file::read(path("Patches", location, ".bps"));
  } else {
    patch = file::read(path("Patches", location, ".bps"));
  }
  if(!patch) return false;

  string manifest;
  string error;
  if(auto output = Beat::Single::apply(input, patch, manifest, error)) {
    if(!error) {
      input = move(*output);
      return true;
    }
  }

  MessageDialog({
    error, "\n\n",
    "Please ensure you are using the correct (headerless) ROM for this patch."
  }).setAlignment(*presentation).error();
  return false;
}
