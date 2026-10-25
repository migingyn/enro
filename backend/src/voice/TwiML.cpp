#include "voice/TwiML.hpp"

namespace enro::twiml {

std::string escape(const std::string& text) {
  std::string out;
  for (char c : text) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default: out += c;
    }
  }
  return out;
}

std::string say(const std::string& text, const std::string& voice) {
  if (voice.empty()) return "<Say>" + escape(text) + "</Say>";
  return "<Say voice=\"" + escape(voice) + "\" language=\"en-US\">" + escape(text) + "</Say>";
}

std::string voiceFor(const std::string& persona) {
  // Twilio's built-in Polly voices. A deeper voice for the personas that yell.
  if (persona == "drill_sergeant" || persona == "coach") return "Polly.Matthew";
  return "Polly.Joanna";
}

std::string play(const std::string& url) { return "<Play>" + escape(url) + "</Play>"; }

std::string gatherSpeech(const std::string& action_url, const std::string& inner) {
  // Speech or one keypad digit (1 = I'm up, 0 = stop). actionOnEmptyResult posts silence back too, so the
  // route decides what happens next instead of Twilio falling off the end of the document.
  return "<Gather input=\"speech dtmf\" action=\"" + escape(action_url) +
         "\" method=\"POST\" numDigits=\"1\" timeout=\"4\" speechTimeout=\"auto\" actionOnEmptyResult=\"true\" hints=\"" +
         escape("I'm up, I am up, five more minutes, ten more minutes, snooze, leaving, stop") + "\">" + inner +
         "</Gather>";
}

std::string redirect(const std::string& url) { return "<Redirect method=\"POST\">" + escape(url) + "</Redirect>"; }

std::string hangup() { return "<Hangup/>"; }

std::string response(const std::string& inner) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Response>" + inner + "</Response>";
}

}  // namespace enro::twiml
