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

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import org.xmlrpc.android.*;

// This AsyncTask downloads the active bug list.
public class TracBugListTask extends TracBackend {
	private Date lastUpdated;
	private long trackerId;
	public TracBugListTask(Context context,
						   long trackerId,
						   String url,
						   String username,
						   String password,
						   Date trackerLastSync) {
		super(context, url, username, password);

		if (trackerLastSync != null)
		{
			this.lastUpdated = trackerLastSync;
		}
		else
		{
        	SimpleDateFormat df = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ");
			try {
				this.lastUpdated = df.parse("1970-01-01T12:32:41+0000");
			} catch (ParseException e) {
				e.printStackTrace();
			}
		}
		this.trackerId = trackerId;
	}
	
	public int getBugList() throws EntomologistException
	{
		//Log.d("Entomologist", "getBugList()");
		Object[] array;
    	SimpleDateFormat df = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss+0000");
    	Calendar calendar = Calendar.getInstance();
    	calendar.setTime(lastUpdated);
    	calendar.add(Calendar.SECOND, 1); 
		//String updateStr = df.format(lastUpdated);
    	String updateStr = df.format(calendar.getTime());
		//Log.d("Entomologist", "Last sync: " + updateStr);
		String baseQuery = "max=0&modified=" + updateStr + "&status=new|accepted|assigned|reopened";		
		// Fetch closed bugs on updates, so we can remove them from the database
		if (lastUpdated.getYear() != 70)
			baseQuery = baseQuery + "|closed";
		
		try
		{
			HashMap<String, Object> cc = new HashMap<String, Object>();
			Object[] ccArray = new Object[1];
			ccArray[0] = new String(baseQuery + "&cc=" + username);
			//Log.d("Entomologist", (String) ccArray[0]);
			cc.put("methodName", "ticket.query");
			cc.put("params", ccArray);

			HashMap<String, Object> assigned = new HashMap<String, Object>();
			Object[] assignedArray = new Object[1];
			assignedArray[0] = new String(baseQuery + "&owner=" + username);
			assigned.put("methodName", "ticket.query");
			assigned.put("params", assignedArray);

			HashMap<String, Object> reported = new HashMap<String, Object>();
			Object[] reportedArray = new Object[1];
			reportedArray[0] = new String(baseQuery + "&reporter=" + username);
			reported.put("methodName", "ticket.query");
			reported.put("params", ccArray);

			
			Object[] paramArray = new Object[3];
			paramArray[0] = cc;
			paramArray[1] = assigned;
			paramArray[2] = reported;
			// This will be a very ugly array of integers coming back:
			// [
			//  [
			//   [ 1,2,3 ]
			//  ],
			//  [
			//   [ 4,5,6 ]
			//  ],
			//  [
			//   [ 7,8,9 ]
			//  ]
			// ]
			array = (Object[]) client.call("system.multicall", paramArray);
		}
		catch (XMLRPCException e)
		{
			e.printStackTrace();
			throw new EntomologistException(e.getMessage());
		}
		
		if (array.length != 3)
			throw new EntomologistException("Something went wrong with trac!");

		Object[] topList1 = (Object[]) array[0];
		Object[] topList2 = (Object[]) array[1];
		Object[] topList3 = (Object[]) array[2];
		
		Log.d("Entomologist", topList1[0].toString());
		Object[] idList1 = (Object[]) topList1[0];
		Object[] idList2 = (Object[]) topList2[0];
		Object[] idList3 = (Object[]) topList3[0];

		Set<Integer> set = new HashSet<Integer>();
		if (idList1.length > 0)
		{
			for (int i = 0; i < idList1.length; ++i)
			{
				Log.d("Entomologist", "Adding ID: " + String.valueOf((Integer)idList1[i]));
				set.add((Integer) idList1[i]);
			}
		}
		
		if (idList2.length > 0)
		{
			for (int i = 0; i < idList2.length; ++i)
			{
				Log.d("Entomologist", "Adding ID: " + String.valueOf((Integer)idList2[i]));

				set.add((Integer) idList2[i]);
			}
		}
		
		if (idList3.length > 0)
		{
			for (int i = 0; i < idList3.length; ++i)
			{
				Log.d("Entomologist", "Adding ID: " + String.valueOf((Integer)idList3[i]));
				set.add((Integer) idList3[i]);
			}
		}
		
		if (set.size() > 0)
			return getBugList(set);
		return 0;
	}
	
	@SuppressWarnings("unchecked")
	public int getBugList(Set<Integer> idList) throws EntomologistException
	{
		int count = 0;
		int updateCount = 0;
		Object[] paramArray = new Object[idList.size()];

		Iterator<Integer> i = idList.iterator();
		while (i.hasNext())
		{
			Integer id = i.next();
			HashMap<String, Object> newMap = new HashMap<String, Object>();
			Object[] idArray = new Object[1];
			idArray[0] = new Integer(id);
			newMap.put("methodName", "ticket.get");
			newMap.put("params", idArray);
			paramArray[count] = newMap;
			count++;
		}
		Object[] response;
		try
		{
			response = (Object[]) client.call("system.multicall", paramArray);
		}
		catch (XMLRPCException e)
		{
			throw new EntomologistException(e.getMessage());
		}
		// "response" will be an Object array looking like this:
		// [ 
		//  [  bug_id,
		//     DateTime creation_time,
		//     DateTime changed_time,
		//     HashMap attributes {
		//       '_ts' - "changetime token", which will be necessary in future versions for updating bugs
		//       'cc'
		//       'changetime' - changed_time again ?,
		//       'component'
		//       'description'
		//       'keywords'
		//       'milestone'
		//       'owner'
		//       'priority'
		//       'reporter'
		//       'resolution'
		//       'status'
		//       'summary'
		//       'time' - creation_time ?
		//       'type' - earlier called "severity"
		//       'version'
		//     }
		//   ],
		//   [ next bug {} ],
		//  ]
		
		if (response.length == 0)
		{
			throw new EntomologistException("The bug tracker returned data I couldn't understand.");
		}
		
//		DBHelper dbHelper = new DBHelper(null);
		Date mostRecentChange = null;
		SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss+0000");

		for (int topLevelSize = 0; topLevelSize < response.length; ++topLevelSize)
		{
			Object[] secondLevel = (Object[]) response[topLevelSize];
			if (secondLevel.length != 1)
				throw new EntomologistException("The bug tracker returned data I couldn't understand.");
			Object[] bug = (Object[]) secondLevel[0];
			if (bug.length != 4)
				throw new EntomologistException("The bug tracker returned data I couldn't understand.");
			
			Integer bugId = (Integer) bug[0];
			Date modified = (Date) bug[2];
			if (mostRecentChange == null)
				mostRecentChange = modified;
			else if (mostRecentChange.before(modified))
				mostRecentChange = modified;
			
			String bugLastModified = sdf.format(modified);
			//Log.v("Entomologist", "Last Modified: " + bugLastModified);
			Map<String, Object> bugMap = (Map<String, Object>) bug[3];
			bugMap.put("last_modified", bugLastModified);
			if (bugMap.get("owner").equals(username))
				bugMap.put("bug_type", "ASSIGNED");
			else if (bugMap.get("reporter").equals(username))
				bugMap.put("bug_type", "REPORTED");
			else
				bugMap.put("bug_type", "CC");
			
			if (lastUpdated.getYear() != 70)
			{
				bugMap.put("ui_notice", "Updated");
				updateCount++;
			}
			
			//Log.d("Entomologist", (String) bugMap.get("owner"));
			dbHelper.deleteBug(trackerId, bugId.intValue());
			if (!bugMap.get("status").equals("closed"))
				dbHelper.insertBug(trackerId, bugId.intValue(), bugMap);
		}
		
		if (lastUpdated.getYear() == 70)
		{
			updateCount = -1;
		}
		
		String lastUpdatedStr = sdf.format(mostRecentChange);
		lastUpdated = mostRecentChange;
		//Log.v("Entomologist", "Setting lastUpdated to " + lastUpdatedStr);
		// Set the "last_updated" tracker value to be the change time of the newest bug in GMT
		dbHelper.updateTrackerLastUpdated(trackerId, lastUpdatedStr);
		return updateCount;
	}
	
	@Override
	protected Void doInBackground(Handler... params) {
		Handler h = params[0];
		Message m = h.obtainMessage();
		Bundle b = new Bundle();
		b.putBoolean("error", false);
		
		try
		{
			int updated = getBugList();
			b.putInt("bugs_updated", updated);
			SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss+0000");
			b.putString("last_update", sdf.format(lastUpdated));
		}
		catch (EntomologistException e)
		{
			Log.e("Entomologist", "EntomologistException caught: " + e.getMessage());
			b.putBoolean("error", true);
			b.putString("errormsg", e.getMessage());
		}
		
		dbHelper.close();
		m.setData(b);
		m.sendToTarget();
		return null;
	}


}
