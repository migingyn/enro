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

std::string say(const std::string& text) {
  // STUB
  // TODO(T02): voice / language attributes per persona.
  return "<Say>" + escape(text) + "</Say>";
}

std::string play(const std::string& url) { return "<Play>" + escape(url) + "</Play>"; }

std::string gatherSpeech(const std::string& action_url, const std::string& inner) {
  // STUB
  // TODO(T02): tune speechTimeout, timeout, hints for snooze phrases.
  return "<Gather input=\"speech\" action=\"" + escape(action_url) + "\" method=\"POST\" speechTimeout=\"auto\">" +
         inner + "</Gather>";
}

std::string redirect(const std::string& url) { return "<Redirect method=\"POST\">" + escape(url) + "</Redirect>"; }

std::string hangup() { return "<Hangup/>"; }

std::string response(const std::string& inner) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Response>" + inner + "</Response>";
}

}  // namespace enro::twiml
