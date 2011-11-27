/*
 *  Entomologist Mobile/Trac
 *  Copyright (C) 2011 Matt Barringer
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 *  Author: Matt Barringer <matt@entomologist-project.org>
 */
package org.entomologistproject.trac;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.Socket;
import java.net.URL;

import org.xmlrpc.android.XMLRPCClient;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Handler;
import android.util.Log;

public class TracBackend extends AsyncTask<Handler, Void, Void>{
	protected String url = "";
	protected String username = "";
	protected String password = "";
	protected String service = "";
	protected Context context;
	protected XMLRPCClient client = null;
	protected DBHelper dbHelper;

	public TracBackend(Context context, String url, String username, String password)
	{
		this.context = context;
		this.dbHelper = new DBHelper(context);
		this.url = url;
		this.username = username;
		this.password = password;
		String host;
		if (url.toLowerCase().startsWith("http://") || url.toLowerCase().startsWith("https://"))
			host = url;
		else
			host = "https://" + url;
		
		if (host.toLowerCase().endsWith("/login/rpc"))
			this.service = host;
		else
			this.service = host + "/login/rpc";
	
		//Log.d("Entomologist", service);
		//Log.d("Entomologist", "Username: " + username + " and " + password);
	}
	
	@SuppressWarnings("unused")
	protected void checkHost() throws EntomologistException
	{
		Socket s = null;
		URL serviceUrl;

		try {
			serviceUrl = new URL(this.service);
		} catch (MalformedURLException e) {
			e.printStackTrace();
			throw new EntomologistException("Malformed URL!");
		}

		Log.d("Entomologist", "Service: " + this.service);
		try {
			if (serviceUrl.getProtocol().equals("http"))
				s = new Socket(serviceUrl.getHost(), 80);
			else
				s = new Socket(serviceUrl.getHost(), 443);
		}
		catch (Exception e)
		{
			if (service.toLowerCase().startsWith("http://"))
				throw new EntomologistException("Could not connect to the server!");
			service = service.toLowerCase().replace("https://", "http://");
			try
			{
				s = new Socket(serviceUrl.getHost(), 80);
			}
			catch (Exception newe)
			{
				throw new EntomologistException("Could not connect to the server!");
			}
		}
		
		try {
			if (s != null)
				s.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		
		this.client = new XMLRPCClient(this.service);
		this.client.setBasicAuthentication(username, password);
		this.client.setUserAgent("Entomologist");
	}
	
	@Override
	protected Void doInBackground(Handler... params) {
		// TODO Auto-generated method stub
		return null;
	}
	
	@Override
	protected void onCancelled()
	{
		dbHelper.close();
	}
}
