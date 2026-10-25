// TwiML builders for the Twilio webhooks. Examples of the expected output are in
// contracts/examples/twiml/. Owner: T02.
#pragma once

#include <string>

namespace enro::twiml {

std::string escape(const std::string& text);  // XML-escape
std::string say(const std::string& text, const std::string& voice = "");
std::string voiceFor(const std::string& persona);  // Polly voice for a persona id
std::string play(const std::string& url);
// <Gather input="speech dtmf" numDigits=1 timeout=4 speechTimeout=auto actionOnEmptyResult>
std::string gatherSpeech(const std::string& action_url, const std::string& inner = "");
std::string redirect(const std::string& url);
std::string hangup();
std::string response(const std::string& inner);  // wraps in <?xml?><Response>...</Response>

}  // namespace enro::twiml
