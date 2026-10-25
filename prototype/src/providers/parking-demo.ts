import twilio from 'twilio';

export const PARKING_DEMO = Object.freeze({
  destination: 'Gilman Parking Structure',
  speed: 1.25,
  voice: 'Google.en-US-Neural2-D' as const,
  speechRate: '125%',
  warning: 'You are walking towards the wrong direction. Please turn around and walk towards Gilman Parking Structure.',
});

function spoken(response: twilio.twiml.VoiceResponse, text: string) {
  response.say({ voice: PARKING_DEMO.voice, language: 'en-US' }, '').prosody({ rate: PARKING_DEMO.speechRate }, text);
}

/** Twilio-hosted continuation contains only this fictional demo's script. */
function continuation(xml: string): string {
  const url = new URL('https://twimlets.com/echo');
  url.searchParams.set('Twiml', xml);
  return url.toString();
}

function listeningTurn(prompt: string, nextXml: string, pauseBefore = 0): string {
  const response = new twilio.twiml.VoiceResponse();
  if (pauseBefore) response.pause({length:pauseBefore});
  const gather = response.gather({input:['speech','dtmf'],action:continuation(nextXml),method:'POST',
    timeout:30,speechTimeout:'1',numDigits:1,actionOnEmptyResult:false,bargeIn:true,language:'en-US'});
  // Listen while the prompt plays too, so a natural early reply isn't lost.
  gather.say({voice:PARKING_DEMO.voice,language:'en-US'},'').prosody({rate:PARKING_DEMO.speechRate},prompt);
  spoken(response, "I didn't hear your reply. Ending this demo for now.");
  response.hangup();
  return response.toString();
}

/** Scripted turns advance on a spoken reply or keypad input, not live GPS classifications. */
export function parkingDemoStages(delaySeconds = 5) {
  if (!Number.isInteger(delaySeconds) || delaySeconds < 0 || delaySeconds > 30) throw new Error('Demo delay must be an integer from zero to thirty seconds.');
  const final = new twilio.twiml.VoiceResponse();
  spoken(final, "Great. You're now walking in the right direction towards Gilman Parking Structure. I can detect that you're walking right now. I'll end the call. Have a great meeting!");
  final.hangup();
  const finalXml = final.toString();

  const suitXml = listeningTurn('Before you go, your email invitation says the dress code is business professional, a full suit. Are you wearing a suit right now?',finalXml);
  const directionXml = listeningTurn(PARKING_DEMO.warning,suitXml,delaySeconds);
  const openingXml = listeningTurn("Hey, it's Enro. I haven't detected movement from your phone. As a reminder, your appointment is at La Jolla Crossroads at six thirty P M, but you haven't started heading out. Your parked car is at Gilman Parking Structure. Let's get moving.",directionXml);
  return {opening:openingXml,direction:directionXml,suit:suitXml,final:finalXml};
}

export function parkingDemoTwiml(delaySeconds = 5): string {
  return parkingDemoStages(delaySeconds).opening;
}
