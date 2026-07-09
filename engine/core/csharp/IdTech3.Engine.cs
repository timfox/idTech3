// IdTech3 engine C# scripting API (compiled with game scripts; GPL-2.0 engine).
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace IdTech3
{
	public static class Engine
	{
		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern void Print( string message );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern string CvarGet( string name );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern void CvarSet( string name, string value );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern int GetMilliseconds();

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern string GetEngineInfo();

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern void Exec( string command );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern bool DbAvailable();

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern string DbPath();

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern bool DbExec( string sql );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern string DbQueryOne( string sql );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern bool ProfileSet( string key, string value );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern string ProfileGet( string key );

		[MethodImpl( MethodImplOptions.InternalCall )]
		public static extern bool ProfileDelete( string key );

		static readonly Dictionary<string, List<Action<string, string, int, int>>> s_handlers =
			new Dictionary<string, List<Action<string, string, int, int>>>( StringComparer.OrdinalIgnoreCase );

		public static void On( string eventName, Action<string, string, int, int> handler )
		{
			if ( string.IsNullOrEmpty( eventName ) || handler == null )
				return;
			List<Action<string, string, int, int>> list;
			if ( !s_handlers.TryGetValue( eventName, out list ) )
			{
				list = new List<Action<string, string, int, int>>();
				s_handlers[eventName] = list;
			}
			list.Add( handler );
		}

		public static void Off( string eventName, Action<string, string, int, int> handler )
		{
			if ( string.IsNullOrEmpty( eventName ) || handler == null )
				return;
			List<Action<string, string, int, int>> list;
			if ( !s_handlers.TryGetValue( eventName, out list ) )
				return;
			list.Remove( handler );
		}

		public static void DispatchEvent( string eventName, string s0, string s1, int i0, int i1 )
		{
			List<Action<string, string, int, int>> list;
			if ( !s_handlers.TryGetValue( eventName, out list ) )
				return;
			var snapshot = list.ToArray();
			foreach ( var h in snapshot )
			{
				try
				{
					h( s0, s1, i0, i1 );
				}
				catch ( Exception ex )
				{
					Print( "C# event " + eventName + " error: " + ex.Message );
				}
			}
		}
	}
}
