package com.oldestmonk.droidsudsolve;

import android.R.color;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Color;
import android.text.style.EasyEditSpan;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Gravity;
import android.widget.Button;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.TextView;
import android.view.ViewGroup.LayoutParams;
import java.util.TreeSet;
import java.util.TreeSet;

import com.oldestmonk.droidsudsolve.R;

public class MainActivity extends Activity {
	private static final int MENU_CLEARALL = 1;
	private static final int MENU_EXITAPP = 2;
	private TextView[][] tvarray;
	private Button[] numbtns;
	int curposinmatrix;
	protected static final int[] bresids={
		R.drawable.back1,
		R.drawable.back2,
		R.drawable.back3,
		R.drawable.back8,
		R.drawable.back9,
		R.drawable.back4,
		R.drawable.back7,
		R.drawable.back6,
		R.drawable.back5
	};
	public MainActivity() {
		tvarray = new TextView[9][9];
		numbtns = new Button[10];
		curposinmatrix = -1;
	
	}
	public native int[]  exactcaller(int[] jproblem);
	static {
        System.loadLibrary("exactcaller");
    }
	protected void setTvBack(int i, int j) {
		
		int ii = i%3;
		int jj = j%3;
		int cresid = bresids[ii+jj*3];
		tvarray[i][j].setBackgroundResource(cresid);
	}
	protected void deactivatePreviousTv(int cpinmat) {
		int i = cpinmat/9;
		int j = cpinmat%9;
		tvarray[i][j].setBackgroundColor(Color.WHITE);
		curposinmatrix = -1;
	}
	protected boolean alContainsVal(TreeSet<Integer> al, int val) {
		 for( int x : al )
		    {
		    if(x==val)
		    	return true;
		    }
		 return false;
	}
	protected void activateTv(int i, int j) {
		if(curposinmatrix!=-1) {
		   deactivatePreviousTv(curposinmatrix);
		}
		tvarray[i][j].setBackgroundColor(Color.YELLOW);
		curposinmatrix = i*9+j;
		for(int u=0; u<9; u++) {
			for(int v=0; v<9; v++) {
				setTvBack(u,v);
			}
		}
		tvarray[i][j].setBackgroundColor(Color.YELLOW);
	}
	int getValAt(int i, int j) {
		String str = tvarray[i][j].getText().toString();
		if(str.equals("")) {
		    return 0;
		}
		else {
			return Integer.parseInt(str);
		}
	}
	TreeSet<Integer> getErrors() {
		TreeSet<Integer> retval = new TreeSet<Integer>();
        int[][] curvalues = new int[9][9];
        for(int i=0; i<9; i++) {
        	for(int j=0; j<9; j++) {
        		curvalues[i][j] = getValAt(i, j);
        	}
        }
		// row and column checks are done by the following for loop
		for(int i=0; i<9; i++) {
		    //check for row i and column i
			for(int j=0; j<9; j++) {
				for(int k=j+1; k<9; k++) {
                   if(curvalues[i][j]!=0 && (curvalues[i][j]== curvalues[i][k])){
                	   retval.add(i*9+j);
                	   retval.add(i*9+k);
                   }
                   if(curvalues[j][i]!=0 && (curvalues[j][i]== curvalues[k][i])){
                	   retval.add(j*9+i);
                	   retval.add(k*9+i);
                   }
				}
			}
		}
		for(int ci=0; ci<3; ci++) {
			for(int cj=0; cj<3; cj++) {
				int cbegini = ci*3;
				int cbeginj = cj*3;
				for(int iti=cbegini; iti<cbegini+3; iti++) {
					for(int itj = cbeginj; itj<cbeginj+3; itj++) {
						if(curvalues[iti][itj]!=0) {
							for(int itii =cbegini; itii<cbegini+3; itii++) {
								for(int itjj = cbeginj; itjj<cbeginj+3; itjj++) {
									if((!(iti==itii && itj==itjj))&&
											(curvalues[itii][itjj]==curvalues[iti][itj])) {
										retval.add(itii*9+itjj);
										retval.add(iti*9+itj);
									}
								}
							}
						}
					}
				}
			}
		}
		return retval;
	}

	protected void checkValuesAndWarn(int curi, int curj) {
		for(int i=0; i<9; i++) {
			for(int j=0; j<9; j++) {
				setTvBack(i, j);
				//tvarray[i][j].setBackgroundResource(R.drawable.back);
				tvarray[i][j].setTextColor(Color.BLACK);
			}
		}
		TreeSet<Integer> al = getErrors();
		for (Integer val : al)  
		{  
			int i = val/9;
			int j = val%9;
			tvarray[i][j].setTextColor(Color.RED);
		}
		if(alContainsVal(al, curi*9+curj)) {
			activateTv(curi, curj);
		}
	}
	protected void changeValueAt(int curposi, int curposj, int val) {
		if(val==0) {
			tvarray[curposi][curposj].setText("");
		}
		else {
		    tvarray[curposi][curposj].setText(String.valueOf(val));
		}
		curposinmatrix = -1;
		checkValuesAndWarn(curposi, curposj);
	}
	protected void handleBtnPress(int i) {
		if(curposinmatrix==-1) {
			AlertDialog.Builder builder = new AlertDialog.Builder(this);
			builder.setMessage("Please click on a cell to select it, then click on a value to enter that value to the cell!")
			       .setCancelable(false)
			       .setPositiveButton("OK", new DialogInterface.OnClickListener() {
			           public void onClick(DialogInterface dialog, int id) {
			                dialog.cancel();
			           }
			       });
			AlertDialog alert = builder.create();
			alert.show();
			return;
		}
		else {
			int curposi = curposinmatrix/9;
			int curposj = curposinmatrix%9;
			changeValueAt(curposi, curposj, i);
		}
	}
	void fillSolution(int[] solution, boolean[] wasempty) {
		for(int n=0; n<81; n++) {
			int i = n/9;
			int j = n%9;
			tvarray[i][j].setText(String.valueOf(solution[n]));
			if(wasempty[n]) {
				tvarray[i][j].setTextColor(Color.GREEN);
			}
		setTvBack(i, j);
		}
	}
	void fillCell(int i, int j, int val) {
		tvarray[i][j].setText(String.valueOf(val));
		tvarray[i][j].setBackgroundColor(Color.GREEN);
	}
	boolean isGoodSol(int[] sol) {
		if(sol[0]==100)
			return false;
		for(int i=0; i<81; i++) {
			if(sol[i]==0)
				return false;
		}
		return true;
	}
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setRequestedOrientation (ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
		setContentView(R.layout.activity_main);

	    TableLayout tl = (TableLayout) findViewById(R.id.sudokutable);
	    
	    for (int j = 0; j < 9; j++) {
	        TableRow tr = new TableRow(this);
//        tr.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT,
//	                LayoutParams.WRAP_CONTENT));
	        tr.setLayoutParams(new TableRow.LayoutParams(
	                LayoutParams.MATCH_PARENT,0,0.1f));
	        for (int i = 0; i < 9; i++) {
	            TextView tView = new TextView(this);
	            
	            tView.setLayoutParams(new TableRow.LayoutParams(0, 
	            		LayoutParams.WRAP_CONTENT, 1f));
	            //tView.setBackgroundResource(R.drawable.back);
	            tView.setGravity(Gravity.CENTER_VERTICAL | Gravity.CENTER_HORIZONTAL);
//	            tView.setLayoutParams(new LayoutParams(
//	                    LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
	            //set it's height = width
	            
	            tr.addView(tView);
	            tvarray[i][j] = tView;
	            
	            int cellht = getResources().getDisplayMetrics().widthPixels/11;
	            tView.setHeight(cellht);
	            setTvBack(i,j);
	            final int finali = i;
	            final int finalj = j;
	            tvarray[i][j].setOnClickListener(new TextView.OnClickListener() {
	            	@Override
	            	public void onClick(View v) {
	            		activateTv(finali,finalj);
	            	}
	            });
	            
	        }
	        tl.addView(tr);
	    }	
	    TableLayout tnt = (TableLayout) findViewById(R.id.numtable);
	    TableRow tr2 = new TableRow(this);
	    tr2.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT,
	    		LayoutParams.WRAP_CONTENT));
	    int bside = getResources().getDisplayMetrics().widthPixels/10;
	    for(int i=1; i<6; i++) {
	    	Button numbtn = new Button(this);
	    	
	    	numbtn.setText(String.valueOf(i));
	    	tr2.addView(numbtn);
	    	numbtns[i] = numbtn;
	    }
	    tnt.addView(tr2);
	    TableRow tr3 = new TableRow(this);
	    tr3.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT,
                LayoutParams.WRAP_CONTENT));
	    for(int i=6; i<10; i++) {
	    	Button numbtn = new Button(this);
	    	
	    	numbtn.setText(String.valueOf(i));
	    	tr3.addView(numbtn);
	    	numbtns[i] = numbtn;
	    }	
	    
	    Button numbtn = new Button(this);
	    
        numbtn.setGravity(Gravity.CENTER_VERTICAL | Gravity.CENTER_HORIZONTAL);
    	numbtn.setText("X");
    	numbtn.setBackgroundColor(Color.RED);
    	tr3.addView(numbtn);
    	numbtns[0] = numbtn;
    	tnt.addView(tr3);
    	
    	for(int i=0; i<10; i++) {
    		final int finali = i;
    		numbtns[i].setOnClickListener(new TextView.OnClickListener() {
    			@Override
    			public void onClick(View v) {
    				handleBtnPress(finali);
    			}
    		}); 
    	}
    	TableRow tr4 = new TableRow(this);
	    tr4.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT,
                LayoutParams.MATCH_PARENT));
    	Button hintbtn = new Button(this);
	    hintbtn.setText("Hint");
	    final Activity factivity = this;
	    hintbtn.setOnClickListener(new Button.OnClickListener() {
	    	@Override
	    	public void onClick(View v) {
	    		TreeSet<Integer> el = getErrors();
	    		if(!el.isEmpty()) {
	    			//show alertdialog
	    			AlertDialog.Builder builder = new AlertDialog.Builder(factivity);
	    			builder.setMessage("Error in your input. Please correct values marked in red!")
	    			       .setCancelable(false)
	    			       .setPositiveButton("OK", new DialogInterface.OnClickListener() {
	    			           public void onClick(DialogInterface dialog, int id) {
	    			                dialog.cancel();
	    			           }
	    			       });
	    			AlertDialog alert = builder.create();
	    			alert.show();
	    			return;
	    		}
	    		if(curposinmatrix==-1 || getValAt(curposinmatrix/9, curposinmatrix%9)!=0) {
	    			AlertDialog.Builder builder = new AlertDialog.Builder(factivity);
	    			builder.setMessage(
	    					"First click on an empty cell to select it," +
	    					" then click on the Hint Button." +
	    					" I will fill that cell with a correct value!")
	    			       .setCancelable(false)
	    			       .setPositiveButton("OK", new DialogInterface.OnClickListener() {
	    			           public void onClick(DialogInterface dialog, int id) {
	    			                dialog.cancel();
	    			           }
	    			       });
	    			AlertDialog alert = builder.create();
	    			alert.show();
	    			return;
	    		}
	    		int ci = curposinmatrix/9;
	    		int cj = curposinmatrix%9;
	    		final int[] jproblem = new int[81];
	    		for(int i=0; i<9; i++) {

	    			for(int j=0; j<9; j++) {
	    				jproblem[i*9+j] = getValAt(i, j);
	    			}
	    		}
	    		
	    		int[] solution = exactcaller(jproblem);
	    		if(isGoodSol(solution)) {
	    			fillCell(ci, cj, solution[ci*9+cj]);
	    		}
	    		else {
	    			AlertDialog.Builder builder = new AlertDialog.Builder(factivity);
	    			builder.setMessage("The puzzle you entered has no solutions!")
	    			       .setCancelable(false)
	    			       .setPositiveButton("OK", new DialogInterface.OnClickListener() {
	    			           public void onClick(DialogInterface dialog, int id) {
	    			                dialog.cancel();
	    			           }
	    			       });
	    			AlertDialog alert = builder.create();
	    			alert.show();
	    			return;
	    		}
	    	}
	    }); 
	    tr4.addView(hintbtn);
	    
	    Button solvebtn = new Button(this);
	    solvebtn.setText("SOLVE!");
	    //final Activity factivity = this;
	    solvebtn.setOnClickListener(new Button.OnClickListener() {
	    	@Override
	    	public void onClick(View v) {
	    		TreeSet<Integer> el = getErrors();
	    		if(!el.isEmpty()) {
	    			//show alertdialog
	    			AlertDialog.Builder builder = new AlertDialog.Builder(factivity);
	    			builder.setMessage("Error in your input. Please correct values marked in red!")
	    			       .setCancelable(false)
	    			       .setPositiveButton("OK", new DialogInterface.OnClickListener() {
	    			           public void onClick(DialogInterface dialog, int id) {
	    			                dialog.cancel();
	    			           }
	    			       });
	    			AlertDialog alert = builder.create();
	    			alert.show();
	    			return;
	    		}
	    		final int[] jproblem = new int[81];
	    		for(int i=0; i<9; i++) {

	    			for(int j=0; j<9; j++) {
	    				jproblem[i*9+j] = getValAt(i, j);
	    			}
	    		}
	    		final boolean[] wasempty = new boolean[81];
	    		for(int i=0; i<81; i++) {
	    			if(jproblem[i]==0) {
	    				wasempty[i] = true;
	    			}
	    			else {
	    				wasempty[i] = false;
	    			}
	    		}
	    		int[] solution = exactcaller(jproblem);
	    		if(isGoodSol(solution)) {
	    			fillSolution(solution, wasempty);
	    		}
	    		else {
	    			AlertDialog.Builder builder = new AlertDialog.Builder(factivity);
	    			builder.setMessage("The puzzle you entered has no solutions!")
	    			       .setCancelable(false)
	    			       .setPositiveButton("OK", new DialogInterface.OnClickListener() {
	    			           public void onClick(DialogInterface dialog, int id) {
	    			                dialog.cancel();
	    			           }
	    			       });
	    			AlertDialog alert = builder.create();
	    			alert.show();
	    			return;
	    		}
	    	}
	    }); 
	    tr4.addView(solvebtn);
	    TableLayout abtnt = (TableLayout) findViewById(R.id.actionbtntable);
	    abtnt.addView(tr4);

	}
	protected void clearAllCells() {
		for(int i=0; i<9; i++) {
			for(int j=0; j<9;j++) {
				tvarray[i][j].setTextColor(Color.BLACK);
				tvarray[i][j].setText("");
				//tvarray[i][j].setBackgroundResource(R.drawable.back);
				setTvBack(i, j);
			}
		}
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		menu.add(0, MENU_CLEARALL, 0, "Clear All");
		menu.add(0, MENU_EXITAPP, 0, "Exit App");
		//getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}
	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {

		case MENU_CLEARALL:
			clearAllCells();
			return true;
		
		case MENU_EXITAPP:
			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(
					this);
			// set title
			alertDialogBuilder.setTitle("Quit?!");

			alertDialogBuilder
			.setMessage("Do you really want to quit?")
			.setCancelable(false)
			.setPositiveButton("Yes",
					new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int id) {
					// if this button is clicked, close
					// current activity
					MainActivity.this.finish();
				}
			})
			.setNegativeButton("No", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int id) {
					dialog.cancel();
				}
			});

			// create alert dialog
			AlertDialog alertDialog = alertDialogBuilder.create();

			// show it
			alertDialog.show();

			return true;
		}
		return false;
	}


}
