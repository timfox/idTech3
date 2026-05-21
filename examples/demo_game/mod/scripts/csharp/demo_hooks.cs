using System;
using IdTech3;

namespace Game
{
	public static class Script
	{
		static int s_frameCount;

		public static void Init()
		{
			Engine.Print( "C# demo_hooks: Init (idTech3 scripting)" );
			Engine.On( "map_load", OnMapLoad );
			Engine.On( "frame", OnFrameEvent );
		}

		public static void Frame( int msec, int realMsec )
		{
			if ( ( s_frameCount++ % 300 ) == 0 )
			{
				Engine.Print( "C# demo_hooks: Frame msec=" + msec );
			}
		}

		static void OnMapLoad( string s0, string s1, int i0, int i1 )
		{
			Engine.Print( "C# demo_hooks: map_load " + s0 );
		}

		static void OnFrameEvent( string s0, string s1, int i0, int i1 )
		{
		}
	}
}
