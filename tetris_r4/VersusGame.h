#pragma once

#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "TetrisGame.h"

namespace tetris {

class VersusGame {
public:
  enum State : uint8_t { CONNECTING, PLAYING, FINISHED };

  void begin(uint32_t now, Game::RandFn rnd) {
    state_ = CONNECTING;
    connectingSince_ = now;
    lastDiscovery_ = 0;
    wifiReady_ = false;
    opponentWon_ = false;
    rnd_ = rnd;
    deviceId_ = macAddressString();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    udp_.begin(WIFI_PORT);
  }

  void update(uint32_t now, Game &game) {
    if (state_ == FINISHED) return;

    if (state_ == CONNECTING) {
      if (WiFi.status() != WL_CONNECTED) {
        if (now - connectingSince_ > WIFI_RETRY_MS) begin(now, rnd_);
        return;
      }

      if (now - lastDiscovery_ >= DISCOVERY_INTERVAL_MS) {
        lastDiscovery_ = now;
        broadcastDiscovery();
      }

      int packet = udp_.parsePacket();
      while (packet) {
        char buffer[PACKET_SIZE] = {0};
        udp_.read(buffer, sizeof(buffer) - 1);
        if (buffer[0] == 'H' &&
            strcmp(buffer + 1, deviceId_.c_str()) != 0) {
          opponentIp_ = udp_.remoteIP();
          wifiReady_ = true;
          state_ = PLAYING;
          game.begin(now, rnd_);
          Serial.print(F("Opponent found at "));
          Serial.println(opponentIp_);
          return;
        }
        packet = udp_.parsePacket();
      }
      return;
    }

    int packet = udp_.parsePacket();
    while (packet) {
      char buffer[PACKET_SIZE] = {0};
      udp_.read(buffer, sizeof(buffer) - 1);
      if (buffer[0] == 'G') {
        game.addGarbage((uint8_t)atoi(buffer + 1), now);
      } else if (buffer[0] == 'L') {
        opponentWon_ = true;
        state_ = FINISHED;
        Serial.println(F("You win! Reset the board to play again."));
      }
      packet = udp_.parsePacket();
    }
  }

  State state() const { return state_; }
  bool opponentWon() const { return opponentWon_; }

  void sendGarbage(uint16_t count) {
    char message[8];
    snprintf(message, sizeof(message), "G%u", count);
    sendPacket(message);
  }

  void sendLoss() {
    sendPacket("L");
    state_ = FINISHED;
  }

private:
  static const char WIFI_SSID[];
  static const char WIFI_PASSWORD[];
  static const uint16_t WIFI_PORT = 4210;
  static const uint32_t DISCOVERY_INTERVAL_MS = 1000;
  static const uint32_t WIFI_RETRY_MS = 5000;
  static const uint8_t PACKET_SIZE = 32;

  String macAddressString() const {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char text[18];
    snprintf(text, sizeof(text),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(text);
  }

  void broadcastDiscovery() {
    char message[PACKET_SIZE];
    snprintf(message, sizeof(message), "H%s", deviceId_.c_str());
    udp_.beginPacket(IPAddress(255, 255, 255, 255), WIFI_PORT);
    udp_.write((const uint8_t *)message, strlen(message));
    udp_.endPacket();
  }

  void sendPacket(const char *message) {
    if (!wifiReady_) return;
    udp_.beginPacket(opponentIp_, WIFI_PORT);
    udp_.write((const uint8_t *)message, strlen(message));
    udp_.endPacket();
  }

  WiFiUDP udp_;
  IPAddress opponentIp_;
  String deviceId_;
  Game::RandFn rnd_ = 0;
  State state_ = CONNECTING;
  bool wifiReady_ = false;
  bool opponentWon_ = false;
  uint32_t connectingSince_ = 0;
  uint32_t lastDiscovery_ = 0;
};

const char VersusGame::WIFI_SSID[] = "TETRIS-VS";       // TODO: Use the actual wifi SSID
const char VersusGame::WIFI_PASSWORD[] = "tetris-vs";   // TODO: Use the actual wifi password

}  // namespace tetris
