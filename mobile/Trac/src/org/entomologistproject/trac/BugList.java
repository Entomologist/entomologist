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

import greendroid.app.GDListActivity;
import greendroid.widget.ActionBar;
import greendroid.widget.ActionBarItem;
import greendroid.widget.NormalActionBarItem;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import org.entomologistproject.trac.R;
import org.entomologistproject.trac.DBHelper;

import android.app.Activity;
import android.app.AlarmManager;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.widget.AdapterView;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

// This is the main activity, showing the list of active bugs
public class BugList extends GDListActivity {
	public static final String PREFS_NAME = "EntomologistPreferences";
	public static final String UPDATE_MESSAGE = "ReloadBugList";
	public static final int UPDATED_BUGS_NOTIFICATION_ID = 1;
	private static final int SETTINGS_ACTIVITY_ID = 1;
	private static final int BUG_DETAIL_ACTIVITY_ID = 2;
	
	private DBHelper dbHelper;
	private Cursor listCursor;
	private AlternateRowCursorAdapter adapter;
	private long trackerRowId;
	private String trackerUrl;
	private String trackerUsername;
	private String trackerPassword;
	private Date trackerLastSync;
	private boolean updateChecked;
	private long updateFrequency;
	private boolean onlyOverWifi;

	private String[] trackerComponents;
	private String[] trackerMilestones;
	private String[] trackerPriorities;
	private String[] trackerResolutions;
	private String[] trackerSeverities;
	private String[] trackerStatuses;
	private String[] trackerVersions;
	private AutoSyncMessageReceiver messageReceiver = new AutoSyncMessageReceiver();

	// When the automatic update is performed, it sends the activity
	// a message to refresh the bug list, if the activity is on top.
	// (Otherwise, the update shows a notification)
	public class AutoSyncMessageReceiver extends BroadcastReceiver
	{
		@Override
		public void onReceive(Context context, Intent intent) {
		    String action = intent.getAction();
		    if (action.equalsIgnoreCase(UPDATE_MESSAGE))
		    {
		    	if (listCursor != null)
		    		listCursor.requery();
		    }
		}
	}
	
	// This handler is called from the bug list sync task
	private class BugsUpdatedHandler extends Handler {
		final ProgressDialog progressDialog;
		
		public BugsUpdatedHandler(ProgressDialog pd)
		{
			progressDialog = pd;
		}
		@Override
		public void handleMessage(Message msg)
		{
			getListView().setEnabled(true);
			Bundle b = msg.getData();
			if (b.getBoolean("error"))
			{
				if (progressDialog != null)
					progressDialog.dismiss();
				Toast toast = Toast.makeText(getApplicationContext(), b.getString("errormsg"), Toast.LENGTH_LONG);
				toast.show();
			}
			else
			{
				String lastUpdate = b.getString("last_update");
				int updateCount = b.getInt("bugs_updated");
	        	SimpleDateFormat df = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss+0000");
	        	try
	        	{
					trackerLastSync = df.parse(lastUpdate);
				}
	        	catch (ParseException e)
	        	{
					e.printStackTrace();
				}

				listCursor.requery();
				if (progressDialog != null)
					progressDialog.dismiss();
				
				// If updateCount is -1, it's from the first sync, so don't toast
				if (updateCount >= 0)
				{
					String toastMessage = "No bugs updated.";
					if (updateCount == 1)
						toastMessage = "Updated " + updateCount + " bug.";
					else if (updateCount > 1)
						toastMessage = "Updated " + updateCount + " bugs.";
					
					Toast toast = Toast.makeText(getApplicationContext(), toastMessage, Toast.LENGTH_LONG);
					toast.show();
				}
			}
		}
	}

	@Override
	public void onDestroy()
	{
		super.onDestroy();
		listCursor.close();
		dbHelper.close();
	}
	
	@Override
	public void onPause()
	{
		super.onPause();
		unregisterReceiver(messageReceiver);
	}
	
	@Override
	public void onResume()
	{
		super.onResume();
		registerReceiver(messageReceiver, new IntentFilter(UPDATE_MESSAGE));
		if (listCursor != null)
		{
			listCursor.requery();
		}
	}
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
		registerReceiver(messageReceiver, new IntentFilter(UPDATE_MESSAGE));

        setContentView(R.layout.main);

        getActionBar().setType(ActionBar.Type.Empty);
        setTitle("Entomologist Mobile/Trac");
        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
    	updateChecked = settings.getBoolean("update_checked", false);
    	updateFrequency = settings.getLong("update_frequency", AlarmManager.INTERVAL_HOUR);
    	onlyOverWifi = settings.getBoolean("only_wifi", true);
    	if (updateChecked)
    		setTimer();
    	
    	
    	String sort = settings.getString("sort_by", "bug_id DESC");
        addActionBarItem(getActionBar()
                .newActionBarItem(NormalActionBarItem.class)
                .setDrawable(R.drawable.ic_menu_refresh),
                R.id.action_bar_refresh);
        addActionBarItem(getActionBar()
        				 .newActionBarItem(NormalActionBarItem.class)
        				 .setDrawable(R.drawable.ic_menu_sort_by_size), 
        				 R.id.action_bar_sort);
        dbHelper = new DBHelper(this);
        Cursor trackerCursor = dbHelper.getTracker();
        if (trackerCursor.getCount() == 0)
        {
        	trackerRowId = -1;
        	trackerUrl = "";
        	trackerUsername = "";
        	trackerPassword = "";
        	SimpleDateFormat df = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss+0000");
        	try
        	{
        		trackerLastSync = df.parse("1970-01-01T12:32:41");
        	}
        	catch (ParseException e)
        	{
				e.printStackTrace();
			}
        }
        else
        {
        	trackerCursor.moveToFirst();
        	trackerRowId = trackerCursor.getLong(0);
        	trackerUrl = trackerCursor.getString(1);
        	trackerUsername = trackerCursor.getString(2);
        	trackerPassword = trackerCursor.getString(3);
        	SimpleDateFormat df = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss+0000");
        	try
        	{
        		//Log.v("Entomologist", "Time: " + trackerCursor.getString(4));
				trackerLastSync = df.parse(trackerCursor.getString(4));
			}
        	catch (ParseException e) {
				e.printStackTrace();
				trackerLastSync = new Date(1970, 1, 1, 12,30);
			}
        }
        
        trackerCursor.close();
    	trackerComponents = dbHelper.getField(trackerRowId, "component");
    	trackerMilestones = dbHelper.getField(trackerRowId, "milestone");
    	trackerPriorities = dbHelper.getField(trackerRowId, "priority");
    	trackerResolutions = dbHelper.getField(trackerRowId, "resolution");
    	trackerSeverities = dbHelper.getField(trackerRowId, "severity");
    	trackerStatuses = dbHelper.getField(trackerRowId, "status");
    	trackerVersions = dbHelper.getField(trackerRowId, "version");

        listCursor = dbHelper.getBugs(sort);
        String[] from = { "bug_id", "summary", "ui_notice"};
        int[] to = { R.id.bugNumberId, R.id.bugSummaryId, R.id.bugUpdated};
        //Log.d("Entomologist", "Setting");
        adapter = new AlternateRowCursorAdapter(BugList.this, R.layout.bug_list_item, listCursor, from, to);
        setListAdapter(adapter);
        ListView view = getListView();
        view.setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View view,
                    int position, long id) {
            			TextView bugNumber = (TextView) view.findViewById(R.id.bugNumberId);
            			Intent intent = new Intent(BugList.this, BugDetails.class);
            			Long bugLong = Long.decode((String) bugNumber.getText());
            			intent.putExtra("bug_id", bugLong.longValue());
            			if (!canConnect(getApplicationContext(), onlyOverWifi))
            				intent.putExtra("no_internet", true);
            			intent.putExtra("tracker_id", trackerRowId);
            			intent.putExtra("tracker_url", trackerUrl);
            			intent.putExtra("tracker_username", trackerUsername);
            			intent.putExtra("tracker_password", trackerPassword);
            			intent.putExtra("components", trackerComponents);
            			intent.putExtra("milestones", trackerMilestones);
            			intent.putExtra("priorities", trackerPriorities);
            			intent.putExtra("resolutions", trackerResolutions);
            			intent.putExtra("severities", trackerSeverities);
            			intent.putExtra("statuses", trackerStatuses);
            			intent.putExtra("versions", trackerVersions);
            			dbHelper.clearUiNotice(trackerRowId, bugLong);
            			startActivityForResult(intent, BUG_DETAIL_ACTIVITY_ID);
                }
              });
        	if (trackerRowId == -1)
        		showSettings();
        	else
        		setTimer();
    }
    
    public void syncBugs(boolean force)
    {
    	if (trackerRowId == -1)
    		return;
    	
    	if (!force && !canConnect(this, onlyOverWifi))
    	{
			Toast toast = Toast.makeText(getApplicationContext(), "You need internet access in order to update your bug list.", Toast.LENGTH_LONG);
			toast.show();
			return;
    	}
    	
    	dbHelper.clearAllUiNotices();
		TracBackend backend = new TracBugListTask(this,
												  trackerRowId,
												  trackerUrl,
												  trackerUsername,
												  trackerPassword,
												  trackerLastSync);
		backend.execute(new BugsUpdatedHandler(ProgressDialog.show(BugList.this, "", 
				"Updating bugs...", true)));
    }
    
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.system_menu, menu);
        return true;
    }
    
    public void setTimer()
    {
    	Intent newIntent = new Intent(BugList.this, UpdateBroadcastReceiver.class);
    	PendingIntent sender = PendingIntent.getBroadcast(this, 1234, newIntent, PendingIntent.FLAG_UPDATE_CURRENT);
    	AlarmManager manager = (AlarmManager) getSystemService(ALARM_SERVICE);
    	
    	// TODO Use newIntent.putExtra here
    	if (updateChecked)
    	{
    		//Log.v("Entomologist", "Setting an alarm");
    		manager.setInexactRepeating(AlarmManager.RTC_WAKEUP,
    									System.currentTimeMillis() + updateFrequency,
    									updateFrequency,
//    									System.currentTimeMillis() + 30000,
//    									30000,
    									sender);
    	}
    	else
    	{
    		//Log.v("Entomologist", "Cancelling alarm");
    		manager.cancel(sender);
    	}
    }
    
    public void setSort(String sort)
    {
        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        SharedPreferences.Editor editor = settings.edit();
        editor.putString("sort_by", sort); 
        editor.commit();
        listCursor.close();
        listCursor = dbHelper.getBugs(sort);
        adapter.changeCursor(listCursor);
        listCursor.requery();
    }
    public void showSortMenu()
    {
    	final CharSequence[] items = {"ID Ascending", "ID Descending", "Last Updated"};

    	AlertDialog.Builder builder = new AlertDialog.Builder(this);
    	builder.setTitle("Sort by");
    	builder.setItems(items, new DialogInterface.OnClickListener() {
    	    public void onClick(DialogInterface dialog, int item) {
    	        if (item == 0)
    	        	setSort("bug_id ASC");
    	        else if (item == 1)
    	        	setSort("bug_id DESC");
    	        else if (item == 2)
    	        	setSort("last_modified DESC");
    	    }
    	});
    	AlertDialog alert = builder.create();
    	alert.show();
    }
    
    @Override
    public boolean onHandleActionBarItemClick(ActionBarItem item, int position) {
    	switch (item.getItemId()) {
    		case R.id.action_bar_refresh:
    			syncBugs(false);
    			break;
    		case R.id.action_bar_sort:
    			showSortMenu();
    			break;
    	}
    	return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
    	//Log.d("Entomologist", "option selected: " + item.getItemId());
    	//Log.d("Entomologist", "vs " + R.id.menu_settings);
    	switch (item.getItemId()) {
    	case R.id.menu_settings:
    		showSettings();
    		break;
    	case R.id.menu_about:
    		showAbout();
    		break;
    	}
    	return super.onOptionsItemSelected(item);
    }
    
    @Override
    protected void onActivityResult (int requestCode, int resultCode, Intent data)
    {
    	if (requestCode == SETTINGS_ACTIVITY_ID)
    	{
	    	if (resultCode == Activity.RESULT_OK)
	    	{
	    		// TODO this should probably handle the case where the url AND
	    		// the update settings have changed
	            Bundle extras = data.getExtras();
                SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
                SharedPreferences.Editor editor = settings.edit();
                onlyOverWifi = extras.getBoolean("only_wifi", true);
                updateChecked = extras.getBoolean("update_checked", true);
                updateFrequency = extras.getLong("update_frequency");
                //Log.v("Entomologist", "Setting new update frequency " + updateFrequency);
                editor.putBoolean("only_wifi", onlyOverWifi);
                editor.putBoolean("update_checked", updateChecked);
                editor.putLong("update_frequency", updateFrequency);
                editor.commit();
                setTimer();
                
	            if (extras.getBoolean("tracker_changed", false))
	            {
	            	String url = extras.getString("tracker_url");
	            	String username = extras.getString("tracker_username");
	            	String password = extras.getString("tracker_password");
	            	if ((url.equals(trackerUrl)) && (username.equals(trackerUsername)) && (password.equals(trackerPassword)))
	            	{
	            		return;
	            	}
	
	            	trackerUrl = url;
	            	trackerUsername = username;
	            	trackerPassword = password;
	            	trackerRowId = extras.getLong("tracker_id");
	            	trackerComponents = extras.getStringArray("tracker_components");
	            	trackerMilestones = extras.getStringArray("tracker_milestones");
	            	trackerPriorities = extras.getStringArray("tracker_priorities");
	            	trackerResolutions = extras.getStringArray("tracker_resolutions");
	            	trackerSeverities = extras.getStringArray("tracker_severities");
	            	trackerStatuses = extras.getStringArray("tracker_statuses");
	            	trackerVersions = extras.getStringArray("tracker_versions");
	
	            	syncBugs(true);
	            }
	    	}
    	}
    	else if (requestCode == BUG_DETAIL_ACTIVITY_ID)
    	{
    		listCursor.requery();
    	}
    }
    
    public void showSettings()
    {
    	//Log.d("Entomologist", "Showing settings");
		Intent intent = new Intent(BugList.this, Settings.class);
		intent.putExtra("tracker_id", trackerRowId);
		intent.putExtra("tracker_url", trackerUrl);
		intent.putExtra("tracker_username", trackerUsername);
		intent.putExtra("tracker_password", trackerPassword);
        intent.putExtra("update_frequency", updateFrequency);
        intent.putExtra("update_checked", updateChecked);
        intent.putExtra("only_wifi", onlyOverWifi);
        
		startActivityForResult(intent, SETTINGS_ACTIVITY_ID);
    }
    
    public static boolean canConnect(Context context, boolean onlyConnectWithWifi)
    {
    	ConnectivityManager manager = (ConnectivityManager) context.getSystemService(CONNECTIVITY_SERVICE);
    	NetworkInfo wifi = manager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);

    	if (wifi.isConnected())
    		return true;
    	
    	if (onlyConnectWithWifi && !wifi.isConnected())
    		return false;
    	
    	NetworkInfo info = manager.getActiveNetworkInfo();
    	if (info == null)
    		return false;
    	return true;
    }
    
    public void showAbout()
    {
		Intent intent = new Intent(BugList.this, About.class);
		startActivity(intent);
    }
}
