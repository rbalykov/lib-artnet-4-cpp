#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ArtNet {

// Constants
constexpr uint16_t ARTNET_PORT = 6454;
constexpr uint16_t ARTNET_HEADER_SIZE = 12;
constexpr uint16_t ARTNET_MAX_DMX_SIZE = 512;

// Op Codes (from spec table 1)
enum class OpCode : uint16_t {
  OpPoll = 0x2000,
  OpPollReply = 0x2100,
  OpDiagData = 0x2300,
  OpCommand = 0x2400,
  OpDataRequest = 0x2700,
  OpDataReply = 0x2800,
  OpDmx = 0x5000,
  OpNzs = 0x5100,
  OpSync = 0x5200,
  OpAddress = 0x6000,
  OpInput = 0x7000,
  OpTodRequest = 0x8000,
  OpTodData = 0x8100,
  OpTodControl = 0x8200,
  OpRdm = 0x8300,
  OpRdmSub = 0x8400,
  // Add more opcodes as needed (but not used in our core implementation)
  // OpVideoSetup = 0xa010,
  // OpVideoPalette = 0xa020,
  // OpVideoData = 0xa040,
  // OpMacMaster = 0xf000,
  // OpMacSlave = 0xf100,
  // OpFirmwareMaster = 0xf200,
  // OpFirmwareReply = 0xf300,
  // OpFileTnMaster = 0xf400,
  // OpFileFnMaster = 0xf500,
  // OpFileFnReply = 0xf600,
  // OplpProg = 0xf800,
  // OplpProgReply = 0xf900,
  // OpMedia = 0x9000,
  // OpMediaPatch = 0x9100,
  // OpMediaControl = 0x9200,
  // OpMediaContrlReply = 0x9300,
  // OpTimeCode = 0x9700,
  // OpTimeSync = 0x9800,
  // OpTrigger = 0x9900,
  // OpDirectory = 0x9a00,
  // OpDirectoryReply = 0x9b00
};

// Common Header
struct ArtHeader {
  std::array<uint8_t, 8> id; // "Art-Net" + null terminator
  uint16_t opcode;           // Low byte first
  uint16_t version;          // Low byte first

  ArtHeader(OpCode code) {
    std::copy(std::begin(std::string("Art-Net")),
              std::end(std::string("Art-Net")), id.begin());
    id[7] = 0;
    opcode = static_cast<uint16_t>(code);
    version = 14; // Assuming Art-Net version 14, will be re-visited on the
                  // sending function.
  }
};

// ArtPoll Packet (from spec section 6.2)
struct ArtPollPacket {
  ArtHeader header;
  uint16_t filler1;
  uint32_t filler2;
  std::array<uint8_t, 4> versionInfo;

  ArtPollPacket()
      : header(OpCode::OpPoll), filler1(0), filler2(0),
        versionInfo{0, 0, 0, 0} {}
};

// ArtPollReply Packet (from spec section 6.3)
struct ArtPollReplyPacket {
  ArtHeader header;
  std::array<uint8_t, 4> ip;
  uint16_t port;
  std::array<uint8_t, 2> versionInfo;
  uint8_t netSwitch;
  uint8_t subSwitch;
  uint16_t oem;
  uint8_t ubeaVersion;
  uint8_t status;
  uint16_t estaMan;
  std::array<uint8_t, 18> shortName;
  std::array<uint8_t, 64> longName;
  std::array<uint8_t, 64> nodeReport;
  uint16_t numPorts;
  std::array<uint8_t, 4> portType;
  std::array<uint8_t, 4> goodOutputA;
  std::array<uint8_t, 4> goodInputA;
  std::array<uint8_t, 4> swIn;
  std::array<uint8_t, 4> swOut;
  std::array<uint8_t, 4> acnPriority;
  std::array<uint8_t, 4> swMacro;
  std::array<uint8_t, 4> swRemote;
  std::array<uint8_t, 3> filler3;

  ArtPollReplyPacket()
      : header(OpCode::OpPollReply), port(ARTNET_PORT), versionInfo{0, 0},
        netSwitch(0), subSwitch(0), oem(0), ubeaVersion(0), status(0),
        estaMan(0), numPorts(0), filler3{0, 0, 0} {
    shortName.fill(0);
    longName.fill(0);
    nodeReport.fill(0);
    portType.fill(0);
    goodOutputA.fill(0);
    goodInputA.fill(0);
    swIn.fill(0);
    swOut.fill(0);
    acnPriority.fill(0);
    swMacro.fill(0);
    swRemote.fill(0);
  }
};

// ArtDmx Packet (from spec section 7.2)
struct ArtDmxPacket {
  ArtHeader header;
  uint8_t sequence;
  uint8_t physical;
  uint16_t universe;
  uint16_t length;
  std::array<uint8_t, ARTNET_MAX_DMX_SIZE> data;
  ArtDmxPacket()
      : header(OpCode::OpDmx), sequence(0), physical(0), universe(0),
        length(0) {
    data.fill(0);
  }
};

// More packet definitions will follow, here is an example:

struct ArtTodDataPacket {
  ArtHeader header;
  uint8_t rdmVer;
  uint8_t port;
  uint8_t spare1;
  uint8_t spare2;
  uint8_t spare3;
  uint8_t spare4;
  uint8_t spare5;
  uint8_t spare6;
  uint8_t net;
  uint8_t command;
  uint8_t addCount;
  std::array<uint8_t, 32> address;
  uint16_t uidTotalHi;
  uint16_t uidTotalLo;
  uint8_t blockCount;
  uint8_t uidCount;
  std::array<uint8_t, 48 * 32> tod;

  ArtTodDataPacket()
      : header(OpCode::OpTodData), rdmVer(0), port(0), spare1(0), spare2(0),
        spare3(0), spare4(0), spare5(0), spare6(0), net(0), command(0),
        addCount(0), uidTotalHi(0), uidTotalLo(0), blockCount(0), uidCount(0) {
    address.fill(0);
    tod.fill(0);
  }
};

} // namespace ArtNet
