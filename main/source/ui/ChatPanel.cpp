#include "ChatPanel.h"
#include "VGUI_KeyCode.h"
#include "VGUI_App.h"
#include "cl_dll/hud.h"
#include <string.h>
#ifdef _WIN32
#include "winsani_in.h"
#include <windows.h>
#include "winsani_out.h"
#endif


using namespace vgui;

const char* ChatPanel::chatModeAll = "say";
const char* ChatPanel::chatModeTeam = "say_team";

ChatPanel::ChatPanel(int x, int y, int wide, int tall)
    : Panel(x, y, wide, tall)
{

    for (int i = 0; i < 256; ++i)
    {
        mKeyPushed[i] = false;
    }
    
}
    
void ChatPanel::SetChatMode(string sChatMode)
{
    mChatMode = sChatMode;
}

void ChatPanel::CancelChat()
{
    
    mText = "";
    setVisible(false);
    
    for (int i = 0; i < 256; ++i)
    {
        mKeyPushed[i] = false;
    }
    
}
//@2014 so many evil inputs
void ChatPanel::KeyDown(int virtualKey, int scanCode)
//void ChatPanel:VGUI::isKeyDown(KeyCode code)
{
    
    if (virtualKey >= 0 && virtualKey < 256)
    {
        mKeyPushed[virtualKey] = true;
    }
    
    if (virtualKey == KEY_ESCAPE)
    {
        CancelChat();
    }
    else if (virtualKey == KEY_ENTER/*VK_RETURN*/)
    {
        
        std::string theCommand;

		theCommand += mChatMode;

		theCommand += " \"";

            // Replace all ';' characters with ':' characters since we can't have
        // ';' characters on a console message.

        for (unsigned int i = 0; i < mText.length(); ++i)
        {
            if (mText[i] == ';')
            {
                mText[i] = ':';
            }
        }
        
        theCommand += mText;

		theCommand += "\"";

		//say_x "the message here" instead of 
		//say_x the message here (ever word was treated as another argument)
        gEngfuncs.pfnClientCmd((char*)theCommand.c_str());

        CancelChat();

    }
    else if (virtualKey == KEY_BACKSPACE/*VK_BACK*/)
    {
        if (mText.length() > 0)
        {
            mText.erase(mText.length() - 1, mText.length());
        }
    }
    else
    {

        byte keyState[256]; // BYTE to byte
       // GetKeyboardState(keyState); evil


        // Turn off caps lock since some people use it for voice comm.

        keyState[KEY_CAPSLOCK]  = 0;

        // Turn off control since some people use it for crouching.

        keyState[KEY_LCONTROL] = 0;
        keyState[KEY_RCONTROL] = 0;
        //@2014 removed keyState[VK_CONTROL]  = 0;

        char buffer[3] = { 0 };
	//@2014 LPWORD is evil
       // int count = toascii(virtualKey, scanCode, keyState, (LPWORD)buffer, 0);
	int count = 1;
        if (count > 0)
        {
            mText += buffer;
        }

    }

}

void ChatPanel::paint()
{

    const AvHFont& theFont = gHUD.GetSmallFont();
    int thePadSize = 5;

    //requestFocus();

    int width;
    int height;

    getSize(width, height);

    int x = thePadSize;
    int y = (height - theFont.GetStringHeight()) / 2;
    
    char prompt[128];
	strncpy(prompt, mChatMode.c_str(), 127);

	prompt[0] = toupper( prompt[0] );

	// Remove any _'s (say_team becomes say team)
	for ( char *pApersand = prompt; pApersand != NULL && *pApersand != 0; pApersand++ )
	{
		// Replace it with a space
		if ( *pApersand == '_' )
			*pApersand = ' ';
	}

	strcat(prompt, ": ");

    x = theFont.DrawString(x, y, prompt, 128, 128, 128);
    
    const char* text = mText.c_str(); 

    while (x + theFont.GetStringWidth(text) > width - thePadSize)
    {
        ++text;
    }

    x = theFont.DrawString(x, y, text, 255, 255, 255);

}

void ChatPanel::paintBackground()
{

    int width;
    int height;

    getSize(width, height);

	drawSetColor(0,0,0, 80);
	drawFilledRect(0,0, width, height);
		
}

bool ChatPanel::WasKeyPushed(int virtualKey) const
{
    if (virtualKey >= 0 && virtualKey < 256)
    {
        return mKeyPushed[virtualKey];
    }
    else
    {
        return true;
    }
}

std::string ChatPanel::UTF8toASCII(unsigned char* multibyte)
{
	string ASCIIValue;
	if (multibyte[0] <= 127)// let ascii pass
		ASCIIValue = multibyte[0];

	if (multibyte[0] == 0xC3)// is it a UTF8 multibyte?
	{
		ASCIIValue = multibyte[1] + 64; 
	}

	return ASCIIValue;
}

void ChatPanel::KeyEvent(int iKeydown)
{
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	bool bTextinput;
	bTextinput = false;
	while (SDL_PollEvent(&event)) {

		if (event.type == SDL_TEXTINPUT)
		{
			unsigned char* buffer;
			buffer = (unsigned char*)event.text.text;

			mText += UTF8toASCII(buffer);
			bTextinput = true;
		}

	}

	if (state[SDL_SCANCODE_ESCAPE])
	{
		CancelChat();

	}
	if (state[SDL_SCANCODE_BACKSPACE])
	{
		if (iKeydown && !bTextinput)
		{
			if (mText.length() > 0)
			{
				mText.erase(mText.length() - 1, mText.length());
			}
		}
	}
	if (state[SDL_SCANCODE_RETURN])
	{
		std::string theCommand;

		theCommand += mChatMode;

		theCommand += " \"";

		// Replace all ';' characters with ':' characters since we can't have
		// ';' characters on a console message.

		for (unsigned int i = 0; i < mText.length(); ++i)
		{
			if (mText[i] == ';')
			{
				mText[i] = ':';
			}
		}

		theCommand += mText;

		theCommand += "\"";

		//say_x "the message here" instead of 
		//say_x the message here (ever word was treated as another argument)
		gEngfuncs.pfnClientCmd((char*)theCommand.c_str());

		CancelChat();
	}
}
