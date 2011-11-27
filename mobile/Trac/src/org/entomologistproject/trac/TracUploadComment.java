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

import java.util.Date;
import java.util.HashMap;
import java.util.Map;

import org.xmlrpc.android.XMLRPCException;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

// POSTs a comment to Trac.  This is called immediately when the user presses the "Save" button
// in the Add a Comment area.
public class TracUploadComment extends TracBackend {
	private String newComment;
	private long bugId;
	private long trackerId;
	public TracUploadComment(Context context,
							 String url,
							 String username,
							 String password,
							 String newComment,
							 long trackerId,
							 long bugId) {
		super(context, url, username, password);
		this.newComment = newComment;
		this.bugId = bugId;
		this.trackerId = trackerId;
	}
	
	@SuppressWarnings("unchecked")
	public void uploadComment() throws EntomologistException
	{
		try {
			// We need to get the _ts info before updating the bug
			Object[] bugValues = (Object[]) client.call("ticket.get", bugId);
			if (bugValues.length != 4)
			{
				//Log.v("Entomologist", "Length: " + bugValues.length);
				throw new EntomologistException("This bug seems to suddenly be invalid!");
				
			}
			
			Map<String, Object> bugMap = (Map<String, Object>) bugValues[3];
			String ts = (String) bugMap.get("_ts");

			Map<String, Object> newMap = new HashMap<String, Object>();
			newMap.put("_ts", ts);
			newMap.put("action", "leave");
			
			client.call("ticket.update", bugId,
					newComment,
					newMap,
					false,
					username);
			
			// TODO: share this code with TracCommentsTask
			Object[] changeLogArray = (Object[]) client.call("ticket.changeLog", bugId);
			if (changeLogArray.length > 0)
			{
				dbHelper.deleteComments(trackerId, bugId);
				for (int i = 0; i < changeLogArray.length; ++i)
				{
					Object[] entry = (Object[]) changeLogArray[i];
					Date entryDate = (Date) entry[0];
					String person = (String) entry[1];
					String type = (String) entry[2];
					String newValue = (String) entry[4];
					if ((type.equals("comment")) && (newValue.length() > 0))
					{
						dbHelper.insertComment(trackerId, bugId, person, newValue, entryDate);
					}
				}
			}
		} catch (XMLRPCException e) {
			  e.printStackTrace();
		      throw new EntomologistException(e.getMessage());
		}

	}
	@Override
	protected Void doInBackground(Handler... params) {
		Handler h = params[0];
		Message m = h.obtainMessage();
		Bundle b = new Bundle();
		b.putBoolean("error", false);
		
		try
		{
			uploadComment();
			b.putBoolean("comment_uploaded", true);
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
