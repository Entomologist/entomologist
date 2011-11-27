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

import org.xmlrpc.android.XMLRPCException;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Base64;
import android.util.Log;

// This AsyncTask downloads attachments.
public class TracDownloadAttachmentTask extends TracBackend {
	private String packageName;
	private String fileName;
	private long bugId;
	public TracDownloadAttachmentTask(Context context, String url, String username,
			String password, String packageName, String fileName, long bugId) {
		super(context, url, username, password);
		this.packageName = packageName;
		this.fileName = fileName;
		this.bugId = bugId;
	}
	
	@SuppressWarnings("unused")
	private String getFile() throws EntomologistException
	{
		try {
			byte[] base64File = (byte[]) client.call("ticket.getAttachment(int ticket, string filename)", bugId, fileName);
			byte[] decodedFile = Base64.decode(base64File, Base64.DEFAULT);
			// TODO: What's the best way to handle temporary files?
			
		} catch (XMLRPCException e) {
			  e.printStackTrace();
		      throw new EntomologistException(e.getMessage());
		} catch (IllegalArgumentException e)
		{
			e.printStackTrace();
		    throw new EntomologistException(e.getMessage());
		}
		return "";
	}

	@Override
	protected Void doInBackground(Handler... params) {
		Handler h = params[0];
		Message m = h.obtainMessage();
		Bundle b = new Bundle();
		b.putBoolean("error", false);
		
		try
		{
			checkHost();
			getFile();
			b.putString("package_name", packageName);
			b.putString("temp_file", fileName);
		}
		catch (EntomologistException e)
		{
			Log.e("Entomologist", "EntomologistException caught: " + e.getMessage());
			b.putBoolean("error", true);
			b.putString("errormsg", e.getMessage());
		}
		
		m.setData(b);
		m.sendToTarget();
		dbHelper.close();

		return null;
	}

}
