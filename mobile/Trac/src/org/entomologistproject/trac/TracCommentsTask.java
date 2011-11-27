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

import org.xmlrpc.android.XMLRPCException;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

// This AsyncTask downloads the comments and (eventually) attachments.
public class TracCommentsTask extends TracBackend {
	private long trackerId;
	private long bugId;
	public TracCommentsTask(Context context,
							long trackerId,
							String url,
							String username,
							String password,
							long bugId) {
		super(context, url, username, password);
		this.trackerId = trackerId;
		this.bugId = bugId;
	}

	private void getComments() throws EntomologistException
	{
		try
		{ 
			Object[] changeLogArray = (Object[]) client.call("ticket.changeLog", bugId);
			//Object[] fileListArray = (Object[]) client.call("ticket.listAttachments", bugId);
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
			/*
			if (fileListArray.length > 0)
			{
				dbHelper.deleteAttachments(trackerId, bugId);
				for (int i = 0; i < fileListArray.length; ++i)
				{
					Object[] fileEntry = (Object[]) fileListArray[i];
					String fileName = (String) fileEntry[0];
					String description = (String) fileEntry[1];
					Integer size = (Integer) fileEntry[2];
					Date timestamp = (Date) fileEntry[3];
					String submitter = (String) fileEntry[4];
					String humanSize = humanReadableByteCount(size.longValue(), true);
					dbHelper.insertAttachment(trackerId, bugId, fileName, description, humanSize, timestamp, submitter);
				}
			} */
		}
		catch (XMLRPCException e)
		{
		  e.printStackTrace();
	      throw new EntomologistException(e.getMessage());
		}
	}
	
	public static String humanReadableByteCount(long bytes, boolean si) {
	    int unit = si ? 1000 : 1024;
	    if (bytes < unit) return bytes + " B";
	    int exp = (int) (Math.log(bytes) / Math.log(unit));
	    String pre = (si ? "kMGTPE" : "KMGTPE").charAt(exp-1) + (si ? "" : "i");
	    return String.format("%.1f %sB", bytes / Math.pow(unit, exp), pre);
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
			getComments();
			b.putBoolean("comments_cached", true);
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
